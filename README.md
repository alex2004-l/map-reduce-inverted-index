
---

## ___Inverted index parallel calculation using the Map-Reduce paradigm___
---

Project realised by: Lache Alexandra Florentina Georgiana, 332CD

---

### Mapper logic

- Initially, when a mapper starts, it checks wheter there are files that haven't been processed yet, and process the first one from the queue.
- The operation of getting and eliminating a filename from the queue must be guarded by a mutex -> so that the same input file won't be processed multiple times.
- Each thread parses its current file, creates an array with `partial results`, and appends it to a vector with all the mapper results. The mapper results are a list with the following format: `{word1, id1}, {word2, id2},...`.
- After there are no more files to process, each thread will finish its routine and the atomic counter for active mapper will decrease. When this counter reaches 0, a broadcast will be sent to the reducer threads that will begin their processing.

---

### Reducer logic

- The reducers start processing after all the mapper threads finish their execution.
- Each reducer will be assigned a part of the pairs {word, id} obtained after the mappers finished processing. They will try to add the in an aggregated database using their first letter as a criteria.
- Since this database is implemented using an `unordered_map`, the approach I used for parallelization was that multiple threads can access the map at the same time, but cannot modify the same value concurrently to avoid race conditions (they will have to get the mutex for the respectiv letter beforehand).
- After this, the reducer threads wait until all the words have been aggregated to their respective lists. 
- The next step is to determine the number of output files each thread will write and retrieving the associated data for the letter from the aggregated results. Before writing the output file, each thread will sort the words by the required criteria.

---

### TL;DR What did I parallize?
- Reading from input files and processing the words
- Aggregating the results
- Writing the output files

---

### Some other implementation details

- Due to the global variables restriction, I created multiple data structures used for sharing datas across the threads. Each structure associated with a thred containts its id, the data shared across all threads and the data shared across the threads of the same type(mapper/receiver).
- When implementing the `aggregated_list` parallelization, I had a race condition cause by trying to add a new pair {key, value} in it. It was resolved by initializing all the possible keys(the alphabet letters beforehand).

---