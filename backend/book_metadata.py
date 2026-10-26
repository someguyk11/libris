"""Bounded, cached Open Library ISBN lookups. Never writes to the catalogue."""
import asyncio
from collections import OrderedDict
from copy import deepcopy
from time import monotonic
import json
import httpx
from isbn import isbn13, isbn10


class MetadataError(Exception):
    def __init__(self, status, message):
        self.status = status
        self.message = message


class OpenLibraryMetadata:
    def __init__(self, transport=None):
        self.transport = transport
        self.cache = OrderedDict()
        self.pending = {}

    async def lookup(self, value):
        canonical = isbn13(value)
        cached = self.cache.get(canonical)
        if cached and cached[0] > monotonic():
            self.cache.move_to_end(canonical)
            return deepcopy(cached[1])
        # Share concurrent lookups for this edition; cache is bounded per worker.
        if canonical not in self.pending:
            self.pending[canonical] = asyncio.create_task(self._load(canonical))
            def cleanup(done):
                if self.pending.get(canonical) is done:
                    self.pending.pop(canonical, None)
                if not done.cancelled():
                    done.exception()  # Retrieve errors even if every HTTP caller disconnected.
            self.pending[canonical].add_done_callback(cleanup)
        task = self.pending[canonical]
        try:
            return deepcopy(await asyncio.shield(task))
        finally:
            if task.done() and self.pending.get(canonical) is task:
                self.pending.pop(canonical, None)

    async def _load(self, canonical):
        try:
            result = await asyncio.wait_for(self._fetch(canonical), timeout=6)
        except (httpx.TimeoutException, TimeoutError):
            raise MetadataError(504, 'Open Library timed out. Try again or enter the book details manually.') from None
        except (httpx.HTTPError, ValueError, TypeError, KeyError):
            raise MetadataError(502, 'Open Library returned an unavailable or invalid response. Enter details manually or try again.') from None
        self.cache[canonical] = (monotonic() + (600 if result['found'] else 60), result)
        self.cache.move_to_end(canonical)
        while len(self.cache) > 256:
            self.cache.popitem(last=False)
        return result

    async def _json(self, client, path, params=None):
        async with client.stream('GET', path, params=params) as response:
            if response.status_code == 429:
                raise MetadataError(429, 'Open Library is busy. Wait a minute before trying again, or enter details manually.')
            if response.status_code == 404:
                return None
            response.raise_for_status()
            body = bytearray()
            async for chunk in response.aiter_bytes():
                body.extend(chunk)
                if len(body) > 1_000_000:
                    raise ValueError('Provider response too large')
            result = json.loads(body)
            if not isinstance(result, dict):
                raise ValueError('Expected an object')
            return result

    async def _fetch(self, canonical):
        async with httpx.AsyncClient(base_url='https://openlibrary.org', timeout=3,
                                     follow_redirects=False, transport=self.transport,
                                     headers={'User-Agent': 'Libris-ISBN/1.0', 'Accept': 'application/json'}) as client:
            key = 'ISBN:' + canonical
            payload = await self._json(client, '/api/books', {'bibkeys': key, 'format': 'json', 'jscmd': 'data'})
            if payload is None or not payload:
                return {'isbn': canonical, 'found': False, 'metadata': {}}
            data = payload.get(key)
            if not isinstance(data, dict):
                raise ValueError('Missing edition response')
            # Edition JSON supplies description/edition only; never substitute a work's ISBN.
            # The Books API's details response avoids /isbn redirects to edition URLs.
            details = await self._json(client, '/api/books', {'bibkeys': key, 'format': 'json', 'jscmd': 'details'}) or {}
            detail_record = details.get(key, {})
            if not isinstance(detail_record, dict):
                raise ValueError('Invalid edition details')
            edition = detail_record.get('details', {})
            if not isinstance(edition, dict):
                raise ValueError('Invalid edition details')
        metadata = {}
        bibliography = {}

        def string(value):
            return value.strip() if isinstance(value, str) and value.strip() else None

        def names(value):
            if not isinstance(value, list):
                return None
            return '; '.join(name for item in value if isinstance(item, dict) and (name := string(item.get('name')))) or None

        for field, value in [('title', string(data.get('title'))), ('author', names(data.get('authors'))),
                             ('publish_date', string(data.get('publish_date')))]:
            if value:
                metadata[field] = value
        for field, value in [('publisher', names(data.get('publishers'))), ('edition', string(edition.get('edition_name')))]:
            if value:
                bibliography[field] = value
        description = edition.get('description', data.get('description'))
        if isinstance(description, dict):
            description = description.get('value')
        if string(description):
            bibliography['description'] = string(description)
        identifiers = data.get('identifiers')
        if isinstance(identifiers, dict) and isinstance(identifiers.get('isbn_10'), list):
            for candidate in identifiers['isbn_10']:
                try:
                    if isbn13(candidate) == canonical:
                        bibliography['isbn10'] = isbn10(candidate)
                        break
                except ValueError:
                    continue
        cover = data.get('cover')
        if isinstance(cover, dict):
            url = string(cover.get('large')) or string(cover.get('medium')) or string(cover.get('small'))
            if url and url.startswith('https://covers.openlibrary.org/'):
                bibliography['cover_url'] = url
        if bibliography:
            metadata['bibliography'] = bibliography
        return {'isbn': canonical, 'found': True, 'metadata': metadata}


metadata_service = OpenLibraryMetadata()
