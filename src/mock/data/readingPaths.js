import { recommendedBooks } from "./books";

export const readingPaths=[
  {
    id:1,
    title:"Introduction to Philosophy",
    description:"Build your foundation — a gentle, ordered way into the questions that have followed us for centuries.",
    steps:5,
    type:"philosophy",
    bookCount:12,
    duration:"6 weeks",
    level:"Beginner",
    quote:"The unexamined life is not worth living.",
    tags:["Foundations","Essay"],
    books:[recommendedBooks[1], recommendedBooks[2], recommendedBooks[0]]
  },
  {
    id:2,
    title:"Modern Science",
    description:"From Newton to the quantum — the ideas, and the people, who redrew reality.",
    steps:6,
    type:"science",
    bookCount:9,
    duration:"4 weeks",
    level:"Intermediate",
    quote:"The important thing is not to stop questioning.",
    tags:["Physics","History"],
    books:[recommendedBooks[2], recommendedBooks[1]]
  }
];