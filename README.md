Nothing to see here. Getting closer though!

- Format Support
    - Do we want to store or write other sample metadata? Author? Patch?

- UI (wait for pyer to do much more)
    - Light skin doesn't suck (and is objeyed in step() in the Skin)
    - Scroll bar on the log would be nice eh
    - Range and Latency and so on controls freeze while rendering

- Engine / Rendering
    - Implement the loop modes not just silence and gate only modes
    - Implement polyphonic rendering
    - Different silence thresholds and keep the new 1e-6 as the default
    - Make sure to include start and end notes in range and just divide top
      and bottom zones accordingly
    - Lower and Upper bound spans "whole keyboard"
    - Label Multisample as "bws/presonus" in the manual at least

- Finishing Touches
    - Write a manual
    - Some code cleanup
        - Widget SC vs SampleCreator
        - Widget in CustomWidgets vs between that and module
        - Module - organize the code more clenaly, add a comment or two
        - Module state machine less of a spaghetti perhaps


- Future Things
    - Decent Sampler could have a UI and Effects by default?
