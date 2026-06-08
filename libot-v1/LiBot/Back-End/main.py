from typing import Any

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles


app = FastAPI(
    title="LiBot Book API",
    description="Fetch book details from Open Library by ISBN.",
    version="1.0.0",
)

OPEN_LIBRARY_ISBN_URL = "https://openlibrary.org/isbn/{isbn}.json"
OPEN_LIBRARY_AUTHOR_URL = "https://openlibrary.org{author_key}.json"
LIBOT_HEADERS = {"User-Agent": "LiBot/1.0 (isbn-lookup-app)"}


app.mount("/static", StaticFiles(directory="static"), name="static")


@app.get("/")
def root() -> FileResponse:
    return FileResponse("static/index.html")


@app.get("/health")
def health() -> dict[str, str]:
    return {
        "message": "LiBot Book API is running",
        "example": "/books/9780140328721",
    }


@app.get("/books/{isbn}")
async def get_book_by_isbn(isbn: str) -> dict[str, Any]:
    clean_isbn = isbn.replace("-", "").replace(" ", "")
    if not clean_isbn.isdigit() or len(clean_isbn) not in (10, 13):
        raise HTTPException(
            status_code=400,
            detail="ISBN must be a valid 10 or 13 digit code.",
        )

    async with httpx.AsyncClient(
        timeout=10.0,
        follow_redirects=True,
        headers=LIBOT_HEADERS,
    ) as client:
        try:
            book_response = await client.get(
                OPEN_LIBRARY_ISBN_URL.format(isbn=clean_isbn)
            )
        except httpx.HTTPError as error:
            raise HTTPException(
                status_code=502,
                detail="Could not connect to Open Library.",
            ) from error

        if book_response.status_code == 404:
            raise HTTPException(
                status_code=404,
                detail="Book not found for this ISBN.",
            )

        if book_response.status_code >= 400:
            raise HTTPException(
                status_code=502,
                detail=f"Open Library returned {book_response.status_code}.",
            )

        book = book_response.json()
        authors = await fetch_authors(client, book.get("authors", []))

    return {
        "isbn": clean_isbn,
        "title": book.get("title"),
        "subtitle": book.get("subtitle"),
        "authors": authors,
        "publishers": book.get("publishers", []),
        "publish_date": book.get("publish_date"),
        "number_of_pages": book.get("number_of_pages"),
        "subjects": book.get("subjects", []),
        "open_library_key": book.get("key"),
        "cover": {
            "small": f"https://covers.openlibrary.org/b/isbn/{clean_isbn}-S.jpg",
            "medium": f"https://covers.openlibrary.org/b/isbn/{clean_isbn}-M.jpg",
            "large": f"https://covers.openlibrary.org/b/isbn/{clean_isbn}-L.jpg",
        },
        "raw": book,
    }


async def fetch_authors(
    client: httpx.AsyncClient,
    author_refs: list[dict[str, str]],
) -> list[dict[str, str | None]]:
    authors: list[dict[str, str | None]] = []

    for author_ref in author_refs:
        author_key = author_ref.get("key")
        if not author_key:
            continue

        try:
            response = await client.get(
                OPEN_LIBRARY_AUTHOR_URL.format(author_key=author_key)
            )
        except httpx.HTTPError:
            authors.append({"key": author_key, "name": None})
            continue
        if response.status_code >= 400:
            authors.append({"key": author_key, "name": None})
            continue

        author = response.json()
        authors.append(
            {
                "key": author_key,
                "name": author.get("name"),
            }
        )

    return authors