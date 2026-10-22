const olCover = (isbn) => `https://covers.openlibrary.org/b/isbn/${isbn}-L.jpg`;

export const continueReading=[
  {
    id:1,
    title:"Atomic Habits",
    author:"James Clear",
    cover:olCover("9780735211292"),
    progress:62,
    pages:320,
    published:2018,
    language:"English",
    genre:"Self Development",
    description:"A proven framework for building good habits and breaking bad ones. Tiny changes, remarkable results."
  },
  {
    id:12,
    title:"The Alchemist",
    author:"Paulo Coelho",
    cover:olCover("9780062315007"),
    progress:68,
    pages:208,
    published:1988,
    language:"English",
    genre:"Fiction",
    description:"A shepherd's journey to follow his personal legend and listen to the language of the world."
  },
  {
    id:6,
    title:"1984",
    author:"George Orwell",
    cover:olCover("9780451524935"),
    progress:42,
    pages:328,
    published:1949,
    language:"English",
    genre:"Classics",
    description:"A chilling vision of totalitarianism, surveillance and the rewriting of truth."
  },
  {
    id:3,
    title:"Sapiens",
    author:"Yuval Noah Harari",
    cover:olCover("9780062316097"),
    progress:25,
    pages:512,
    published:2011,
    language:"English",
    genre:"History",
    description:"From the Stone Age to the present, the defining forces that shaped human history reconsidered."
  },
  {
    id:10,
    title:"The Hobbit",
    author:"J.R.R. Tolkien",
    cover:olCover("9780547928227"),
    progress:54,
    pages:310,
    published:1937,
    language:"English",
    genre:"Fantasy",
    description:"An unexpected journey — Bilbo Baggins is drawn out of his quiet life into the wild, wide world."
  },
  {
    id:13,
    title:"The Kite Runner",
    author:"Khaled Hosseini",
    cover:olCover("9781573222457"),
    progress:37,
    pages:371,
    published:2003,
    language:"English",
    genre:"Fiction",
    description:"An unforgettable portrait of friendship, betrayal and redemption set in a country on the verge of change."
  },
  {
    id:14,
    title:"Norwegian Wood",
    author:"Haruki Murakami",
    cover:olCover("9780375704024"),
    progress:81,
    pages:296,
    published:1987,
    language:"English",
    genre:"Literature",
    description:"A wistful, elegiac novel of memory and love, tracing a young man's reckoning with the past."
  },
  {
    id:15,
    title:"The Midnight Library",
    author:"Matt Haig",
    cover:olCover("9780525559474"),
    progress:19,
    pages:304,
    published:2020,
    language:"English",
    genre:"Fiction",
    description:"Between life and death there is a library — a book of regrets, a chance to choose a different life."
  },
  {
    id:16,
    title:"Thinking, Fast and Slow",
    author:"Daniel Kahneman",
    cover:olCover("9780374533557"),
    progress:88,
    pages:499,
    published:2011,
    language:"English",
    genre:"Psychology",
    description:"Two systems drive the way we think: one fast, one slow — and understanding them changes everything."
  },
  {
    id:17,
    title:"Dune",
    author:"Frank Herbert",
    cover:olCover("9780441172719"),
    progress:46,
    pages:896,
    published:1965,
    language:"English",
    genre:"Science Fiction",
    description:"A desert planet, a young heir, and the spice of destiny — a landmark of modern science fiction."
  },
  {
    id:18,
    title:"Pride & Prejudice",
    author:"Jane Austen",
    cover:olCover("9780141439518"),
    progress:73,
    pages:279,
    published:1813,
    language:"English",
    genre:"Classics",
    description:"Wit, first impressions, and love in a regency world where everything — and nothing — is as it seems."
  }
];

export const recommendedBooks=[
  {
    id:2,
    title:"Deep Work",
    author:"Cal Newport",
    cover:olCover("9781455586691"),
    pages:304,
    published:2016,
    language:"English",
    genre:"Productivity",
    description:"A guide to focused success in a distracted world — and how to reclaim the ability to concentrate."
  },
  {
    id:3,
    title:"Sapiens",
    author:"Yuval Noah Harari",
    cover:olCover("9780062316097"),
    pages:512,
    published:2011,
    language:"English",
    genre:"History",
    description:"From the Stone Age to the present, the defining forces that shaped human history reconsidered."
  },
  {
    id:4,
    title:"The Alchemist",
    author:"Paulo Coelho",
    cover:olCover("9780062315007"),
    pages:208,
    published:1988,
    language:"English",
    genre:"Fiction",
    description:"A shepherd's journey to follow his personal legend and listen to the language of the world."
  },
  {
    id:5,
    title:"The Psychology of Money",
    author:"Morgan Housel",
    cover:olCover("9780857197689"),
    pages:256,
    published:2020,
    language:"English",
    genre:"Finance",
    description:"How our relationship with money is shaped less by intelligence and more by behavior."
  },
  {
    id:6,
    title:"1984",
    author:"George Orwell",
    cover:olCover("9780451524935"),
    pages:328,
    published:1949,
    language:"English",
    genre:"Classics",
    description:"A chilling vision of totalitarianism, surveillance and the rewriting of truth."
  },
  {
    id:7,
    title:"The Hobbit",
    author:"J.R.R. Tolkien",
    cover:olCover("9780547928227"),
    pages:310,
    published:1937,
    language:"English",
    genre:"Fantasy",
    description:"An unexpected journey — Bilbo Baggins is drawn out of his quiet life into the wild, wide world."
  },
  {
    id:8,
    title:"Ikigai",
    author:"Héctor García",
    cover:olCover("9781786330895"),
    pages:208,
    published:2016,
    language:"English",
    genre:"Wellbeing",
    description:"The Japanese secret to a long and happy life, drawn from the rhythms of daily purpose."
  },
  {
    id:9,
    title:"Thinking, Fast and Slow",
    author:"Daniel Kahneman",
    cover:olCover("9780374533557"),
    pages:499,
    published:2011,
    language:"English",
    genre:"Psychology",
    description:"Two systems drive the way we think: one fast, one slow — and understanding them changes everything."
  },
  {
    id:10,
    title:"The Midnight Library",
    author:"Matt Haig",
    cover:olCover("9780525559474"),
    pages:304,
    published:2020,
    language:"English",
    genre:"Fiction",
    description:"Between life and death there is a library — a book of regrets, a chance to choose a different life."
  },
  {
    id:11,
    title:"Pride and Prejudice",
    author:"Jane Austen",
    cover:olCover("9780141439518"),
    pages:279,
    published:1813,
    language:"English",
    genre:"Classics",
    description:"Wit, first impressions, and love in a regency world where everything — and nothing — is as it seems."
  },
  {
    id:12,
    title:"Dune",
    author:"Frank Herbert",
    cover:olCover("9780441172719"),
    pages:896,
    published:1965,
    language:"English",
    genre:"Science Fiction",
    description:"A desert planet, a young heir, and the spice of destiny — a landmark of modern science fiction."
  }
];

export const mostBorrowed=[
  {id:9,title:"1984",author:"George Orwell",cover:olCover("9780451524935")},
  {id:8,title:"Ikigai",author:"Héctor García",cover:olCover("9781786330895")},
  {id:10,title:"The Hobbit",author:"J.R.R. Tolkien",cover:olCover("9780547928227")},
  {id:11,title:"The Alchemist",author:"Paulo Coelho",cover:olCover("9780062315007")},
  {id:12,title:"Atomic Habits",author:"James Clear",cover:olCover("9780735211292")},
  {id:13,title:"The Psychology of Money",author:"Morgan Housel",cover:olCover("9780857197689")},
  {id:14,title:"Sapiens",author:"Yuval Noah Harari",cover:olCover("9780062316097")},
  {id:15,title:"Pride and Prejudice",author:"Jane Austen",cover:olCover("9780141439518")},
  {id:16,title:"Dune",author:"Frank Herbert",cover:olCover("9780441172719")}
];