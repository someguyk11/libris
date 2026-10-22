import { recommendedBooks } from "./books";

const olCover = (isbn) =>
  `https://covers.openlibrary.org/b/isbn/${isbn}-L.jpg`;

/* ==========================================================
   CURATED FOR YOU
   A small, cross-genre shelf for the opening rail.
   ========================================================== */

export const curatedForYou = [
  {
    id: 101,
    title: "Project Hail Mary",
    author: "Andy Weir",
    cover: olCover("9780593135204"),
    pages: 476,
    published: 2021,
    language: "English",
    genre: "Science Fiction",
    description:
      "An amnesiac astronaut wakes alone on a spacecraft with a single impossible mission — and finds an unlikely partner in the void."
  },
  {
    id: 102,
    title: "The Silent Patient",
    author: "Alex Michaelides",
    cover: olCover("9781250301697"),
    pages: 336,
    published: 2019,
    language: "English",
    genre: "Mystery",
    description:
      "A woman shoots her husband and never speaks again. A criminal psychotherapist is determined to make her talk."
  },
  {
    id: 103,
    title: "Educated",
    author: "Tara Westover",
    cover: olCover("9780399590504"),
    pages: 334,
    published: 2018,
    language: "English",
    genre: "Biography",
    description:
      "Born to survivalists in the mountains of Idaho, a young woman leaves her isolated world for the shock of a first classroom."
  },
  {
    id: 104,
    title: "Circe",
    author: "Madeline Miller",
    cover: olCover("9780316556347"),
    pages: 393,
    published: 2018,
    language: "English",
    genre: "Fantasy",
    description:
      "A daughter of Helios, exiled to a lonely island, discovers her power and reforges her own myth in a world of gods."
  },
  {
    id: 105,
    title: "The Midnight Library",
    author: "Matt Haig",
    cover: olCover("9780525559474"),
    pages: 304,
    published: 2020,
    language: "English",
    genre: "Fiction",
    description:
      "Between life and death there is a library — a book of regrets, a chance to try every life you might have lived."
  },
  {
    id: 106,
    title: "Atomic Habits",
    author: "James Clear",
    cover: olCover("9780735211292"),
    pages: 320,
    published: 2018,
    language: "English",
    genre: "Self Development",
    description:
      "A proven framework for building good habits and breaking bad ones. Tiny changes, remarkable results."
  }
];

/* ==========================================================
   NEW ARRIVALS
   Fresh on the shelves.
   ========================================================== */

export const newArrivals = [
  {
    id: 201,
    title: "Klara and the Sun",
    author: "Kazuo Ishiguro",
    cover: olCover("9780593318171"),
    pages: 303,
    published: 2021,
    language: "English",
    genre: "Fiction",
    description:
      "An artificial friend watches the world from a storefront window, hoping to be chosen — and to understand what love is."
  },
  {
    id: 202,
    title: "The Ministry for the Future",
    author: "Kim Stanley Robinson",
    cover: olCover("9780316300131"),
    pages: 576,
    published: 2020,
    language: "English",
    genre: "Science Fiction",
    description:
      "A speculative look at the decades ahead, from climate action and policy to the technologies that could rescue our future."
  },
  {
    id: 203,
    title: "Lessons in Chemistry",
    author: "Bonnie Garmus",
    cover: olCover("9780385547345"),
    pages: 400,
    published: 2022,
    language: "English",
    genre: "Fiction",
    description:
      "A brilliant chemist turned reluctant TV cook shows 1960s America — and everyone else — exactly what she's made of."
  },
  {
    id: 204,
    title: "Where the Crawdads Sing",
    author: "Delia Owens",
    cover: olCover("9780735219090"),
    pages: 384,
    published: 2018,
    language: "English",
    genre: "Mystery",
    description:
      "A reclusive girl from the marsh becomes the prime suspect in a murder that has haunted her coastal town for years."
  },
  {
    id: 205,
    title: "Becoming",
    author: "Michelle Obama",
    cover: olCover("9781524763138"),
    pages: 448,
    published: 2018,
    language: "English",
    genre: "Biography",
    description:
      "An intimate memoir of a first lady — from a South Side childhood to the White House, told with honesty and grace."
  },
  {
    id: 206,
    title: "The Body Keeps the Score",
    author: "Bessel van der Kolk",
    cover: olCover("9780143127741"),
    pages: 464,
    published: 2014,
    language: "English",
    genre: "Psychology",
    description:
      "How trauma reshapes the brain and the body, and the paths to recovery that can help us begin again."
  }
];

/* ==========================================================
   GENRES — Explore by genre
   Backed by the CollectionCard art gradients.
   ========================================================== */

export const discoverGenres = [
  {
    id: 301,
    title: "Fantasy",
    description: "Magic, myth and adventure — worlds where the impossible becomes the everyday.",
    type: "fantasy",
    bookCount: 64,
    tags: ["Epic", "Mythology"],
    books: [recommendedBooks[5], recommendedBooks[8]]
  },
  {
    id: 302,
    title: "Mystery",
    description: "Closed rooms, whispered motives and puzzles that refuse to stay solved.",
    type: "mystery",
    bookCount: 41,
    tags: ["Thriller", "Noir"],
    books: [curatedForYou[1], newArrivals[3]]
  },
  {
    id: 303,
    title: "Science Fiction",
    description: "Distant worlds and near futures — the literature of what if.",
    type: "science",
    bookCount: 57,
    tags: ["Space", "Speculative"],
    books: [recommendedBooks[10], curatedForYou[0]]
  },
  {
    id: 304,
    title: "Classics",
    description: "Timeless stories that shaped how we read, written to outlast their generation.",
    type: "classics",
    bookCount: 38,
    tags: ["Literature", "Modern"],
    books: [recommendedBooks[4], recommendedBooks[9]]
  },
  {
    id: 305,
    title: "Biography",
    description: "Extraordinary lives, told from the inside — memory made book.",
    type: "biography",
    bookCount: 29,
    tags: ["Memoir", "History"],
    books: [curatedForYou[2], newArrivals[4]]
  },
  {
    id: 306,
    title: "Philosophy",
    description: "Big questions, well argued — a quiet room for how to live.",
    type: "philosophy",
    bookCount: 22,
    tags: ["Essays", "Ideas"],
    books: [recommendedBooks[1], recommendedBooks[7]]
  }
];

/* ==========================================================
   READING PATHS
   ========================================================== */

export const discoverReadingPaths = [
  {
    id: 1,
    title: "Beginnings of Philosophy",
    description: "Pull up a chair — a gentle, ordered way into the questions we've been asking for centuries.",
    steps: 5,
    type: "philosophy",
    bookCount: 12,
    duration: "6 weeks",
    level: "Beginner",
    quote: "The unexamined life is not worth living.",
    tags: ["Foundations", "Essay"],
    books: [recommendedBooks[1], recommendedBooks[2], recommendedBooks[0]],
    previewBooks: [recommendedBooks[1], recommendedBooks[2], recommendedBooks[0]]
  },
  {
    id: 2,
    title: "Modern Science",
    description: "From hidden systems to the edge of the cosmos — the books that explain how the world works.",
    steps: 6,
    type: "science",
    bookCount: 9,
    duration: "4 weeks",
    level: "Intermediate",
    quote: "The important thing is not to stop questioning.",
    tags: ["Physics", "History"],
    books: [curatedForYou[0], recommendedBooks[10], newArrivals[1]],
    previewBooks: [curatedForYou[0], recommendedBooks[10], newArrivals[1]]
  },
  {
    id: 3,
    title: "Classic Journeys",
    description: "Timeless novels that reshaped storytelling — best read in the order they changed literature.",
    steps: 6,
    type: "history",
    bookCount: 15,
    duration: "5 weeks",
    level: "Intermediate",
    quote: "It's a dangerous business, Frodo, going out your door.",
    tags: ["Classics", "Adventure"],
    books: [recommendedBooks[4], recommendedBooks[9], curatedForYou[4]],
    previewBooks: [recommendedBooks[4], recommendedBooks[9], curatedForYou[4]]
  },
  {
    id: 4,
    title: "Slow Evenings",
    description: "Comfortable, gorgeous reads for winding down — no deadlines, just pleasure.",
    steps: 3,
    type: "default",
    bookCount: 6,
    duration: "2 weeks",
    level: "Beginner",
    quote: "And suddenly all the books seemed to be books about the world.",
    tags: ["Slow Reading", "Fiction"],
    books: [curatedForYou[3], newArrivals[2], curatedForYou[4]],
    previewBooks: [curatedForYou[3], newArrivals[2], curatedForYou[4]]
  }
];

/* ==========================================================
   CURATOR COLLECTIONS
   ========================================================== */

export const discoverCuratedCollections = [
  {
    id: 401,
    title: "Books That Changed the World",
    description: "A small shelf of works whose ideas travelled further than their covers — influential across entire generations.",
    type: "history",
    curator: "Libris Editorial",
    bookCount: 18,
    tags: ["History", "Influence"],
    books: [recommendedBooks[1], recommendedBooks[4], recommendedBooks[2]]
  },
  {
    id: 402,
    title: "Quiet Classics",
    description: "Timeless stories worth revisiting slowly, chosen for voice, patience, and the pleasure of a well-kept sentence.",
    type: "literature",
    curator: "Elena Marsh",
    bookCount: 24,
    tags: ["Literature", "Slow Reading"],
    books: [recommendedBooks[9], curatedForYou[4]]
  },
  {
    id: 403,
    title: "Thinkers Who Defined Us",
    description: "Books that rewire the way you think — essays and ideas for a sharper, kinder mind.",
    type: "philosophy",
    curator: "Libris Editorial",
    bookCount: 12,
    tags: ["Ideas", "Essay"],
    books: [recommendedBooks[7], recommendedBooks[0], curatedForYou[5]]
  },
  {
    id: 404,
    title: "Words to Carry",
    description: "Memoirs and first-person accounts so vivid they feel like keeping company.",
    type: "poetry",
    curator: "Amara Osei",
    bookCount: 15,
    tags: ["Memoir", "Voice"],
    books: [curatedForYou[2], newArrivals[4]]
  }
];

/* ==========================================================
   MOST BORROWED
   ========================================================== */

export const discoverMostBorrowed = [
  recommendedBooks[1],
  recommendedBooks[4],
  recommendedBooks[5],
  recommendedBooks[6],
  recommendedBooks[10],
  recommendedBooks[2]
];