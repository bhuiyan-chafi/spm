## LoopTest

With n = 4096 and loop order i,j,k we achieved a time execution of 1357.75 seconds. For order i,k,j 336.656 and k,i,j 336.003.

## Why vector<T>, not traditional data types?

We can use but vectors allow us to to do more when the size is dynamic. Traditional data-type operations usually live in the call stack while vector by default lives in the Heap(dynamic size allocation is performed automatically). On the otherhand it's safe for value copying, assigning and freeing from the memory.
