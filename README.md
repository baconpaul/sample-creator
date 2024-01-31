Nothing to see here.

Big swaths of work until there is though

- Audio Thread
  - Don't do the IO on the audio thread, dummy
  - Also block out the writes into chunks to
    improve performance
- Format Support
  - mono wav if no input given
  - Is the inst block correct? Endpoint check etc
  - SFZ file support
  - bws/presonus multisample xml
- UI
  - Show the keyboard as a 'rpgoress bar'
  - Shrink the log a lot
  - Value fields for notes gets typein support
  - VU Meter / Recording Status
  - An actual design! Lets hope Steve bites :)
- Engine / Rendering
  - "Drone Mode" - what is it and how would it work?
  - Loop Points
  - A "Latency Mode" for when resampling host
  - Optionally render polyphonically to save time?
