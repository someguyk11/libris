export const statuses = ["Available", "Borrowed", "Reserved", "Maintenance"]
export const shelfCategories = ["General", "Books", "Reference", "Periodicals"]

export function validateShelf(input, existing = []) {
  const shelf = Object.fromEntries(["code", "name", "category", "section", "description", "aisle", "row"].map(key => [key, String(input[key] || "").trim()]))
  shelf.code = shelf.code.toUpperCase()
  if (!/^[A-Z0-9-]{2,20}$/.test(shelf.code)) throw new Error("Use 2–20 letters, numbers, or hyphens for the shelf identifier.")
  if (existing.includes(shelf.code)) throw new Error("That shelf identifier is already registered.")
  if (!shelf.name || shelf.name.length > 100) throw new Error("Enter a shelf name of up to 100 characters.")
  if (!shelfCategories.includes(shelf.category)) throw new Error("Choose a shelf category.")
  if (!shelf.section || shelf.section.length > 100) throw new Error("Enter a library section of up to 100 characters.")
  if (shelf.description.length > 500 || shelf.aisle.length > 40 || shelf.row.length > 40) throw new Error("Shorten the optional location details.")
  return shelf
}
const day = 86400000
const relativeDate = (offset, now) => new Date(now + offset * day).toISOString().slice(0, 10)

export function createAdminSeed(now = Date.now()) {
  const rows = [
    ["Dune", "Frank Herbert", "9780441172719", "Science Fiction", "SH-B03", "Available"],
    ["Sapiens", "Yuval Noah Harari", "9780062316097", "History", "SH-A01", "Borrowed"],
    ["The Alchemist", "Paulo Coelho", "9780062315007", "Fiction", "SH-B02", "Available"],
    ["Atomic Habits", "James Clear", "9780735211292", "Self Help", "SH-C01", "Available"],
    ["1984", "George Orwell", "9780451524935", "Dystopian", "SH-B01", "Reserved"],
    ["To Kill a Mockingbird", "Harper Lee", "9780061120084", "Classics", "SH-A03", "Available"],
    ["Clean Code", "Robert C. Martin", "9780132350884", "Programming", "SH-C02", "Maintenance"],
    ["The Midnight Library", "Matt Haig", "9780525559474", "Fiction", "SH-B04", "Available"],
    ["Educated", "Tara Westover", "9780399590504", "Memoir", "SH-A01", "Borrowed"],
    ["Project Hail Mary", "Andy Weir", "9780593135204", "Science Fiction", "SH-B03", "Available"],
    ["The Silent Patient", "Alex Michaelides", "9781250301697", "Mystery", "SH-B02", "Reserved"],
    ["The Hobbit", "J.R.R. Tolkien", "9780547928227", "Fantasy", "SH-B01", "Available"],
    ["Deep Work", "Cal Newport", "9781455586691", "Self Help", "SH-C01", "Available"],
    ["Norwegian Wood", "Haruki Murakami", "9780375704024", "Fiction", "SH-B04", "Available"]
  ]
  const members = ["Aarav Mehta", "Mira Shah", "Rohit Sharma", "Ananya Rao", "Elena Marsh", "Amara Osei"].map((name, i) => ({ id: `member-${i + 1}`, name, email: `${name.toLowerCase().replaceAll(" ", ".")}@example.com`, joined: new Date(now - (i + 1) * day).toISOString() }))
  const books = rows.map((row, i) => ({ id: `book-${i + 1}`, title: row[0], author: row[1], isbn: row[2], genre: row[3], shelf: row[4], status: row[5], language: "English", cover: `https://covers.openlibrary.org/b/isbn/${row[2]}-M.jpg`, added: new Date(now - i * day).toISOString(), archived: false, due: row[5] === "Borrowed" ? relativeDate(i === 8 ? -1 : 3, now) : "", borrowerId: row[5] === "Borrowed" ? members[i % members.length].id : "" }))
  const events = []
  for (let i = 29; i >= 0; i--) {
    for (const [type, count] of [["loan", 2 + (i * 7 % 5)], ["return", 1 + (i * 3 % 4)]]) {
      events.push({ id: `seed-${i}-${type}`, type, count, title: `${count} books ${type === "loan" ? "borrowed" : "returned"}`, detail: "Daily circulation", at: new Date(now - i * day - 3600000).toISOString() })
    }
  }
  return { version: 1, books, members, shelves: [...new Set(rows.map(row => row[4]))].sort(), events }
}

export function parseCsv(text) {
  const rows = []; let row = []; let field = ""; let quoted = false
  const source = text.replace(/^\uFEFF/, "")
  for (let i = 0; i < source.length; i++) {
    const char = source[i]
    if (char === '"') { if (quoted && source[i + 1] === '"') { field += '"'; i++ } else if (!quoted && field) throw new Error("A quote must start at the beginning of a CSV field."); else quoted = !quoted }
    else if (char === "," && !quoted) { row.push(field.trim()); field = "" }
    else if ((char === "\n" || char === "\r") && !quoted) { if (char === "\r" && source[i + 1] === "\n") i++; row.push(field.trim()); if (row.some(Boolean)) rows.push(row); row = []; field = "" }
    else field += char
  }
  if (quoted) throw new Error("The CSV has an unclosed quoted field.")
  row.push(field.trim()); if (row.some(Boolean)) rows.push(row)
  if (rows.length < 2) throw new Error("Include a header row and at least one book.")
  const headers = rows.shift().map(value => value.toLowerCase())
  for (const key of ["title", "author", "isbn"]) if (!headers.includes(key)) throw new Error(`Missing required column: ${key}.`)
  if (new Set(headers).size !== headers.length) throw new Error("The CSV contains duplicate column names.")
  return rows.map((values, index) => {
    if (values.length !== headers.length) throw new Error(`Row ${index + 2} has ${values.length} fields; expected ${headers.length}.`)
    return Object.fromEntries(headers.map((key, i) => [key, values[i]]))
  })
}

export function validateBook(input, existing = [], editingId = null) {
  const book = { ...input, title: String(input.title || "").trim(), author: String(input.author || "").trim(), isbn: String(input.isbn || "").replace(/[\s-]/g, ""), genre: String(input.genre || "General").trim(), language: String(input.language || "English").trim(), status: input.status || "Available", shelf: String(input.shelf || "").trim() }
  if (!book.title || !book.author) throw new Error("Title and author are required.")
  if (!/^(?:\d{13}|\d{9}[\dXx])$/.test(book.isbn)) throw new Error("Enter an ISBN with 10 or 13 characters.")
  if (existing.some(item => item.isbn === book.isbn && item.id !== editingId)) throw new Error(`ISBN ${book.isbn} is already in your catalogue, including archived titles.`)
  if (!statuses.includes(book.status)) throw new Error(`Unknown status: ${book.status}.`)
  return book
}

export function circulationSeries(events, days, now = Date.now()) {
  return Array.from({ length: days }, (_, index) => {
    const date = new Date(now); date.setHours(0, 0, 0, 0); date.setDate(date.getDate() - days + index + 1)
    const end = new Date(date); end.setDate(end.getDate() + 1)
    const matches = events.filter(event => new Date(event.at) >= date && new Date(event.at) < end)
    return { date: date.toISOString(), label: date.toLocaleDateString(undefined, days <= 7 ? { weekday: "short" } : { month: "short", day: "numeric" }), loans: matches.filter(event => event.type === "loan").reduce((sum, event) => sum + (event.count || 1), 0), returns: matches.filter(event => event.type === "return").reduce((sum, event) => sum + (event.count || 1), 0) }
  })
}

export function toCsv(books) {
  const keys = ["title", "author", "isbn", "genre", "shelf", "status", "language"]
  const escape = value => '"' + String(value ?? "").replace(/^[=+@-]/, "'$&").replaceAll('"', '""') + '"'
  return [keys.join(","), ...books.map(book => keys.map(key => escape(book[key])).join(","))].join("\r\n")
}
