Overview
========

* **gzip.(c|h)** 
  
  Implementation of a zip-this-file Function. 
  In libmoosecat this used to zip the backup of the Database written to Disk

  This is mostly taken from Zlib-Docs.

* **gasyncqueue-watch.(c|h):** 
  
  A GSource that is able to watch a GAsyncQueue. It is added to the Mainloop and
  uses Polling to watch the length of the Queue. If the Queue has been filled
  with one ore more elements a callback is called, where the watched Queue is
  passed. The Polling is timeoutbased and tries to be clever by calculation the
  next timeout on base of earlier timeouts - adapting to fast dispatching if
  needed, or cpu-saving idling.
  
  Code originally from Rythmbox, but it was pretty much rewritten to be more
  flexible.

  This is used to dispatch signals on the main thread.

* **paths.(c|h)** 
  
  Misc functions to handle paths. Nothing exciting here (yet).

  Currently there is only a function to determine the depth of a path:

    depth('/home/chris')  == 2
    depth('/home/chris/') == 2

* **sleep-grained.(c|h):**

  Polling Sleep that wakes up periodically and checks for a condition. 
  If true it continues to sleep, if false it will stop.

  This is used to periodically wake up the ping thread.

* **job-manager.(c|h):**
  
  Threading Helper used throughout libmoosecat.

  Provides an easy way to create a working Thread and send Jobs to it.
  Only one job at a time is processed, and the caller can query with a job-id
  OA  it's status, or fetch the result returned by the execution-thread.
