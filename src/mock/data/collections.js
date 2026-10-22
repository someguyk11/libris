import { recommendedBooks } from "./books";

export const collections=[
  {
    id:1,
    title:"Fantasy",
    description:"Magic, myths and adventure — worlds where the impossible becomes the everyday.",
    bookCount:64,
    type:"fantasy",
    tags:["Epic","Mythology"],
    books:[recommendedBooks[6], recommendedBooks[3]]
  },
  {
    id:2,
    title:"Classics",
    description:"Timeless stories that shaped how we read, written to outlast their generation.",
    bookCount:38,
    type:"classics",
    tags:["Literature","Modern"],
    books:[recommendedBooks[4], recommendedBooks[3]]
  },
  {
    id:3,
    title:"Ideas",
    description:"A space for big questions — the books that quietly change the way you think.",
    bookCount:42,
    type:"science",
    tags:["Essays","History"],
    books:[recommendedBooks[1], recommendedBooks[0], recommendedBooks[2]]
  }
];