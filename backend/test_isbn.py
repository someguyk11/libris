"""Pure ISBN and mocked provider tests: no live network or database access."""
import asyncio
import unittest
import httpx
from isbn import normalize_isbn, isbn13, isbn10
from book_metadata import OpenLibraryMetadata, MetadataError

ISBN = '9780306406157'
KEY = 'ISBN:' + ISBN


class ISBNTests(unittest.TestCase):
    def test_valid_formats_and_equivalent_editions(self):
        self.assertEqual(normalize_isbn(' 0-306-40615-2\n'), '0306406152')
        self.assertEqual(isbn13('0 306 40615 2'), ISBN)
        self.assertEqual(isbn13('978-0-306-40615-7'), ISBN)
        self.assertEqual(isbn13('0-8044-2957-x'), '9780804429573')
        self.assertEqual(isbn10('9780804429573'), '080442957X')
        self.assertEqual(isbn13('9791090636071'), '9791090636071')
        self.assertIsNone(isbn10('9791090636071'))

    def test_checksums_malformed_whitespace_and_non_isbn_barcodes(self):
        for value in ('0306406153', '9780306406158', '0804429570', '978030640615',
                      '4006381333931', 'ISBN 0306406152', '０３０６４０６１５２', '123X456789', '', None, ' ' * 65):
            with self.subTest(value=value), self.assertRaises(ValueError):
                isbn13(value)


class MetadataTests(unittest.IsolatedAsyncioTestCase):
    async def test_maps_only_provider_fields_preserves_edition_and_caches_aliases(self):
        requests = []
        def handler(request):
            requests.append(request)
            self.assertEqual(request.url.host, 'openlibrary.org')
            if request.url.params['jscmd'] == 'details':
                return httpx.Response(200, json={KEY: {'details': {'description': {'value': 'Provider description'}, 'edition_name': 'Second edition'}}})
            return httpx.Response(200, json={KEY: {'title': 'Provider title', 'authors': [{'name': 'A'}, {'name': 'B'}],
                'publish_date': '1980', 'publishers': [{'name': 'Publisher'}], 'identifiers': {'isbn_10': ['080442957X', '0306406152']},
                'cover': {'large': 'https://covers.openlibrary.org/b/id/1-L.jpg'}}})
        service = OpenLibraryMetadata(httpx.MockTransport(handler))
        result = await service.lookup('0306406152')
        self.assertEqual(result['metadata']['author'], 'A; B')
        self.assertEqual(result['metadata']['bibliography']['isbn10'], '0306406152')
        self.assertEqual(result['metadata']['bibliography']['description'], 'Provider description')
        result['metadata']['title'] = 'Local correction'
        self.assertEqual((await service.lookup(ISBN))['metadata']['title'], 'Provider title')
        self.assertEqual(len(requests), 2)

    async def test_sparse_metadata_does_not_invent_values(self):
        service = OpenLibraryMetadata(httpx.MockTransport(lambda request: httpx.Response(200, json={KEY: {'title': 'Only title'}} if request.url.params['jscmd'] == 'data' else {})))
        self.assertEqual((await service.lookup(ISBN))['metadata'], {'title': 'Only title'})

    async def test_not_found_is_cached_without_fetching_details(self):
        requests = []
        def handler(request):
            requests.append(request)
            return httpx.Response(200, json={})
        service = OpenLibraryMetadata(httpx.MockTransport(handler))
        self.assertFalse((await service.lookup(ISBN))['found'])
        self.assertFalse((await service.lookup('0306406152'))['found'])
        self.assertEqual(len(requests), 1)

    async def test_provider_failure_modes_are_safe_and_not_cached(self):
        for response, status in [(httpx.Response(429), 429), (httpx.Response(500), 502),
                                 (httpx.Response(200, content='not json'), 502),
                                 (httpx.Response(200, json=[]), 502),
                                 (httpx.Response(200, json={KEY: []}), 502),
                                 (httpx.Response(200, content=b'x' * 1_000_001), 502)]:
            with self.subTest(status=status):
                service = OpenLibraryMetadata(httpx.MockTransport(lambda _: response))
                with self.assertRaises(MetadataError) as caught:
                    await service.lookup(ISBN)
                self.assertEqual(caught.exception.status, status)
                self.assertNotIn('Traceback', caught.exception.message)
                self.assertFalse(service.cache)
                self.assertFalse(service.pending)

    async def test_timeout_network_error_and_not_found(self):
        for error, status in [(httpx.ReadTimeout('private provider error'), 504), (httpx.ConnectError('secret network detail'), 502)]:
            def handler(_):
                raise error
            with self.assertRaises(MetadataError) as caught:
                await OpenLibraryMetadata(httpx.MockTransport(handler)).lookup(ISBN)
            self.assertEqual(caught.exception.status, status)
            self.assertNotIn(str(error), caught.exception.message)
        self.assertFalse((await OpenLibraryMetadata(httpx.MockTransport(lambda _: httpx.Response(404))).lookup(ISBN))['found'])

    async def test_concurrent_alias_requests_are_coalesced_and_cancellation_is_safe(self):
        entered, release = asyncio.Event(), asyncio.Event()
        calls = 0
        async def handler(_):
            nonlocal calls
            calls += 1
            entered.set()
            await release.wait()
            return httpx.Response(200, json={})
        service = OpenLibraryMetadata(httpx.MockTransport(handler))
        first = asyncio.create_task(service.lookup(ISBN))
        await entered.wait()
        second = asyncio.create_task(service.lookup('0306406152'))
        first.cancel()
        with self.assertRaises(asyncio.CancelledError):
            await first
        release.set()
        self.assertFalse((await second)['found'])
        self.assertEqual(calls, 1)
        self.assertFalse(service.pending)

    async def test_expired_cache_reloads_and_capacity_is_bounded(self):
        service = OpenLibraryMetadata(httpx.MockTransport(lambda _: httpx.Response(200, json={})))
        for index in range(256):
            service.cache[str(index)] = (float('inf'), {'found': False})
        service.cache[ISBN] = (0, {'found': True})
        self.assertFalse((await service.lookup(ISBN))['found'])
        self.assertEqual(len(service.cache), 256)
        self.assertNotIn('0', service.cache)
