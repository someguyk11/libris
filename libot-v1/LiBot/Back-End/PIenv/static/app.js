const form = document.querySelector("#book-form");
const input = document.querySelector("#isbn-input");
const quickPicks = document.querySelectorAll("[data-isbn]");

const views = {
  empty: document.querySelector("#empty-state"),
  loading: document.querySelector("#loading-state"),
  error: document.querySelector("#error-state"),
  card: document.querySelector("#book-card"),
};

const fields = {
  cover: document.querySelector("#cover-image"),
  isbn: document.querySelector("#book-isbn"),
  title: document.querySelector("#book-title"),
  subtitle: document.querySelector("#book-subtitle"),
  authors: document.querySelector("#book-authors"),
  publishers: document.querySelector("#book-publishers"),
  date: document.querySelector("#book-date"),
  pages: document.querySelector("#book-pages"),
  subjects: document.querySelector("#book-subjects"),
  link: document.querySelector("#library-link"),
};

form.addEventListener("submit", (event) => {
  event.preventDefault();
  searchBook(input.value);
});

quickPicks.forEach((button) => {
  button.addEventListener("click", () => {
    input.value = button.dataset.isbn;
    searchBook(button.dataset.isbn);
  });
});

async function searchBook(isbn) {
  const cleanIsbn = isbn.trim();

  if (!cleanIsbn) {
    showError("Please enter an ISBN before searching.");
    input.focus();
    return;
  }

  showView("loading");

  try {
    const response = await fetch(`/books/${encodeURIComponent(cleanIsbn)}`);
    const data = await response.json();

    if (!response.ok) {
      throw new Error(data.detail || "Could not fetch this book.");
    }

    renderBook(data);
  } catch (error) {
    showError(error.message);
  }
}

function renderBook(book) {
  fields.cover.src = book.cover?.large || "";
  fields.cover.alt = book.title ? `${book.title} cover` : "Book cover";
  fields.isbn.textContent = `ISBN ${book.isbn}`;
  fields.title.textContent = book.title || "Untitled book";
  fields.subtitle.textContent = book.subtitle || "";
  fields.subtitle.classList.toggle("hidden", !book.subtitle);
  fields.authors.textContent = listNames(book.authors, "Unknown author");
  fields.publishers.textContent = listValues(book.publishers, "Unknown publisher");
  fields.date.textContent = book.publish_date || "Unknown";
  fields.pages.textContent = book.number_of_pages || "Not listed";
  fields.link.href = book.open_library_key
    ? `https://openlibrary.org${book.open_library_key}`
    : `https://openlibrary.org/isbn/${book.isbn}`;

  renderSubjects(book.subjects || []);
  showView("card");
}

function renderSubjects(subjects) {
  fields.subjects.innerHTML = "";
  const visibleSubjects = subjects.slice(0, 8);

  if (visibleSubjects.length === 0) {
    const fallback = document.createElement("span");
    fallback.textContent = "No subjects listed";
    fields.subjects.append(fallback);
    return;
  }

  visibleSubjects.forEach((subject) => {
    const item = document.createElement("span");
    item.textContent = subject;
    fields.subjects.append(item);
  });
}

function listNames(items, fallback) {
  const names = items
    .map((item) => item.name)
    .filter(Boolean);

  return listValues(names, fallback);
}

function listValues(items, fallback) {
  return items.length > 0 ? items.join(", ") : fallback;
}

function showError(message) {
  views.error.textContent = message;
  showView("error");
}

function showView(name) {
  Object.entries(views).forEach(([viewName, element]) => {
    element.classList.toggle("hidden", viewName !== name);
  });
}
