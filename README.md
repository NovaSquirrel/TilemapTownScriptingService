# TilemapTownScriptingService
Scripting service for [Tilemap Town](https://github.com/NovaSquirrel/TilemapTown/) using [Luau](https://luau.org/)

This program manages multiple Luau virtual machines and accepts commands given over stdin to start and stop individual scripts, run callbacks, receive values from API calls, and check the status of running scripts. It will also send messages over stdout to let the host program know about API calls, send error messages, send statuses, and give the results of status checks. Status messages to print on the console are sent over stderr.

Each user gets a single virtual machine to themselves, with a RAM usage cap shared across all of their simultaneously running scripts. Each virtual machine contains any number of scripts (usually it's one script per in-world entity) and each script contains any number of threads; user code can split off additional threads, and a thread is started in response to each callback. The service will run through all running running scripts and all running threads and give each of them a millisecond at most to run before preempting them and letting something else run.

If a single user thread takes too much time it will be forced to sleep and get a strike, and if it gets enough then the thread will be terminated, and if that happens too many times the whole script is just stopped.