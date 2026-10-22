const olCover = (isbn) => `https://covers.openlibrary.org/b/isbn/${isbn}-L.jpg`;

export const waitlistBooks = [
  {
    id: 101,
    title: "The Midnight Library",
    author: "Matt Haig",
    cover: olCover("9780525559474"),
    details: ["#2 in queue", "Waiting for return"],
    waitlistPosition: 2,
    waitlistStatus: "Waiting for return"
  },
  {
    id: 102,
    title: "The Silent Patient",
    author: "Alex Michaelides",
    cover: olCover("9781501192270"),
    details: ["#4 in queue", "Waiting for return"],
    waitlistPosition: 4,
    waitlistStatus: "Waiting for return"
  },
  {
    id: 103,
    title: "Atomic Habits",
    author: "James Clear",
    cover: olCover("9780735211292"),
    details: ["#1 in queue", "Almost ready"],
    waitlistPosition: 1,
    waitlistStatus: "Almost ready"
  },
  {
    id: 104,
    title: "Project Hail Mary",
    author: "Andy Weir",
    cover: olCover("9780593135204"),
    details: ["#3 in queue", "Waiting for return"],
    waitlistPosition: 3,
    waitlistStatus: "Waiting for return"
  },
  {
    id: 105,
    title: "The Seven Husbands of Evelyn Hugo",
    author: "Taylor Jenkins Reid",
    cover: olCover("9781501161933"),
    details: ["#5 in queue", "Waiting for return"],
    waitlistPosition: 5,
    waitlistStatus: "Waiting for return"
  }
];

export const borrowedBooks = [
  {
    id: 1,
    title: "Atomic Habits",
    author: "James Clear",
    cover: olCover("9780735211292"),
    progress: 62,
    progressLabel: "Due in 8 days",
    actionLabel: "Continue Reading"
  },
  {
    id: 2,
    title: "The Alchemist",
    author: "Paulo Coelho",
    cover: olCover("9780062315007"),
    progress: 68,
    progressLabel: "Due in 14 days",
    actionLabel: "Continue Reading"
  },
  {
    id: 4,
    title: "Sapiens",
    author: "Yuval Noah Harari",
    cover: olCover("9780062316097"),
    progress: 25,
    progressLabel: "Due in 21 days",
    actionLabel: "Continue Reading"
  },
  {
    id: 6,
    title: "1984",
    author: "George Orwell",
    cover: olCover("9780451524935"),
    progress: 42,
    progressLabel: "Due in 5 days",
    actionLabel: "Continue Reading"
  }
];

export const readingBooks = [
  {
    id: 1,
    title: "Atomic Habits",
    author: "James Clear",
    cover: olCover("9780735211292"),
    progress: 62,
    actionLabel: "Continue Reading"
  },
  {
    id: 2,
    title: "The Alchemist",
    author: "Paulo Coelho",
    cover: olCover("9780062315007"),
    progress: 68,
    actionLabel: "Continue Reading"
  },
  {
    id: 3,
    title: "1984",
    author: "George Orwell",
    cover: olCover("9780451524935"),
    progress: 42,
    actionLabel: "Continue Reading"
  },
  {
    id: 4,
    title: "Sapiens",
    author: "Yuval Noah Harari",
    cover: olCover("9780062316097"),
    progress: 25,
    actionLabel: "Continue Reading"
  },
  {
    id: 5,
    title: "Project Hail Mary",
    author: "Andy Weir",
    cover: olCover("9780593135204"),
    progress: 15,
    actionLabel: "Continue Reading"
  }
];

export const userCollections = [
  {
    id: 1,
    title: "Quiet Evenings",
    description: "Cozy reads for winding down",
    count: 12,
    type: "default"
  },
  {
    id: 2,
    title: "Things I Want to Understand",
    description: "Big questions, deep answers",
    count: 8,
    type: "science"
  },
  {
    id: 3,
    title: "Classics",
    description: "Timeless stories that endure",
    count: 15,
    type: "classics"
  },
  {
    id: 4,
    title: "Books for Later",
    description: "When time slows down",
    count: 21,
    type: "default"
  }
];

export const savedBooks = [
  {
    id: 10,
    title: "Project Hail Mary",
    author: "Andy Weir",
    cover: olCover("9780593135204")
  },
  {
    id: 11,
    title: "The Seven Husbands of Evelyn Hugo",
    author: "Taylor Jenkins Reid",
    cover: olCover("9781501161933")
  },
  {
    id: 12,
    title: "Educated",
    author: "Tara Westover",
    cover: olCover("9780399590528")
  },
  {
    id: 13,
    title: "The Midnight Library",
    author: "Matt Haig",
    cover: olCover("9780525559474")
  },
  {
    id: 14,
    title: "Klara and the Sun",
    author: "Kazuo Ishiguro",
    cover: olCover("9780593318171")
  },
  {
    id: 15,
    title: "Tomorrow, and Tomorrow, and Tomorrow",
    author: "Gabrielle Zevin",
    cover: olCover("9780593321201")
  }
];