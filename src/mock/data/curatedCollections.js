import { recommendedBooks } from "./books";

export const curatedCollections=[
  {
    id:1,
    title:"Books That Changed History",
    description:"A small shelf of works whose ideas travelled further than their covers — influential across entire generations.",
    type:"history",
    curator:"Libris Editorial",
    tags:["History","Influence"],
    books:[recommendedBooks[2], recommendedBooks[4], recommendedBooks[1]]
  },
  {
    id:2,
    title:"Quiet Classics",
    description:"Timeless stories worth revisiting slowly, chosen for voice, patience, and the pleasure of a well-kept sentence.",
    type:"literature",
    curator:"Libris Editorial",
    tags:["Literature","Slow Reading"],
    books:[recommendedBooks[3], recommendedBooks[6]]
  }
];