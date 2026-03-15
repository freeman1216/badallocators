## What is this?
A collection of lightweight header-only  allocator implemetations
## Includes
- Fixed sized arena
- Growing arena
- Slabs (bitmask & freelist)
- Buddy (w & w/o bitmask optimisation)
## How to use it  
1. Include the nessesary header in your project  
2. Define nessesary "IMPLEMENTATION" and include the header 

```c
#define BAD_BBUDDY_IMPLEMENTATION
#include "badbbuddy.h"
```

3. Alocate the map either statically or dinamically 
```c
BAD_BBUDDY_ALLOCATE_STATIC(sbbuddy, 12, 6);

```
Or
```c
char * dyn_buddy = malloc(BAD_BBUDY_ALLOC_SIZE(12,6));
bad_bbuddy *dbuddy = bad_bbuddy_init_allocate(dyn_buddy,BAD_BBUDDY_GET_FREELIST_PTR(dyn_buddy),
                                             BAD_BBUDDY_GET_BMASK_PTR(dyn_buddy,12,6)12,6);
```
4. Use the api
```c
void* b1 = bad_bbuddy_alloc(sbbuddy, 64); // 64 bytes
bad_bbuddy_free(sbbuddy, b1, 64);
```
## Notes
Api casts pointers so make sure to compile the TU with implemetation with strict aliasing disabled

The examples folder provides usage examples

You can freely modify or copy whatever you need.
