import { continueReading, recommendedBooks } from './books.js'

// A separate guest demo catalogue. Locations and availability are illustrative.
const source = [...continueReading, ...recommendedBooks]
const entries = [
  ['The Alchemist', '9780062315007', 'Fiction', 'F-03', 'Shelf 4', true],
  ['Atomic Habits', '9780735211292', 'Personal growth', 'P-02', 'Shelf 2', true],
  ['Dune', '9780441172719', 'Science fiction', 'S-01', 'Shelf 3', true],
  ['Project Hail Mary', '9780593135204', 'Science fiction', 'S-01', 'Shelf 5', true],
  ['The Midnight Library', '9780525559474', 'Fiction', 'F-02', 'Shelf 1', false],
  ['1984', '9780451524935', 'Classics', 'C-01', 'Shelf 2', true]
]
export const guestBooks = entries.map(([title, isbn, section, aisle, shelf, available]) => {
  const book = source.find(item => item.title === title)
  return {
    id: title.toLowerCase().replace(/[^a-z0-9]+/g, '-'), title, isbn,
    author: book?.author || 'Andy Weir',
    description: book?.description || 'A lone astronaut must solve an impossible scientific mystery to save humanity, with help from an unexpected friend.',
    cover: `/guide-assets/${isbn}.jpg`, edition: 'Paperback edition',
    available, section, aisle, shelf, location: `${section} → ${aisle} → ${shelf}`,
    directions: `Follow the ${section.toLowerCase()} signs to aisle ${aisle}. Look for the label “${shelf}”. Ask library staff if you need assistance.`
  }
})
export const guestAtlases = [
  { id: 'Atlas-01', available: true, battery: 96 },
  { id: 'Atlas-02', available: true, battery: 87 },
  { id: 'Atlas-03', available: false, battery: 12 }
]
