# Measurement Tool

Goal and mission statement: Devide the myth from reality

## Final product:
A measurement VST plugin that loads upto 4 other plugins and had several measurement units for different types of plugins (EQ, Dynamics, delay, reverb). It has a pool of testsignals inside, but can also render audio from DAW through all plugins in parallel and you can switch between the algorithms (delay, gain and so on are equalized inside). It also has an automatic paramter matcher. But the user can open the GUI and define parameters to compare or as starting points for the optimization. 

## Persona that uses this tool:
### Simple musician with some technical interest
• User reads about a new plugin
• downloads demo and want to compare this new plugin against others he/she already owns
==> Easy to use

### PLugin Developer:
• develops algorithm and want to check everything and compare to competitors

⇒ Something like pluginval but more for the processing not just specification


## Feature List (for the final product):
- load up to 4 plugins in parallel and can display their GUI in separate windows
- render audio from DAW or files from disk or test signals to all plugins in parallel, output could be saved to disk or listened to (gapless switching, users choice)
- switch between the plugins in real time (gapless)
- measure several different aspects of the plugins (e.g. frequency response, phase, group delay, THD, THD+N, noise, SNR, crosstalk, null test with alignment), easy to add new measurements
- display transfer function (and perhaps other measurements) in real-time (continously updated, by using a click signal or a short sweep for example), so that the user can adjust the parameters of the plugins and see the effect in real time
- automatic parameter matcher to match one plugin to another (e.g. match a free EQ to a commercial EQ). Start point can be set by the user or the tool can find a good starting point automatically. The user can define which parameters to match and which to ignore. The user can also define the frequency range to match and the tolerance for the matching. The tool can also display the deviation between the two plugins in real time.
- user-friendly GUI (easy page, expert page, developer page) with different levels of detail and complexity. The user can choose which measurements to display and which to hide.  The user can also save and load presets for the measurements.
- 


## Development path to get there:

This is a huge project, so the development should be divided into as independent parts as possible, so each can be optimized separately.

### The VST host part. 

This plugin loads other plugin, so it is a complete VST host in itself.
Consequences:
- a step for the development is a stand alone version of the host:


### necessary pre tests for the final product:


#### Simple Stand-alone host (to learn how to load plugins and render audio through them)
- can load audio signals from disk and render them through the plugins: the output can be saved to disk or listen to.
- can scan for plugins and show them in a list
- can load one plugin and show its parameters in a list
- can load one plugin and show its GUI in a window
- can load several plugins and show their GUI in separate windows
- can render audio through multiple plugins in parallel
- can switch between the plugins and listen to the output of each plugin without stopping the audio engine (gapless switching)
- can measure latency and gain of each plugin and compensate for it in the output
- can save the output of each plugin to a separate file
- has a simple GUI to control the host and the plugins
- is based on JUCE and can be compiled for Windows, Mac and Linux
- only VST3
- VST2 support via https://github.com/pierreguillot/FTS

This tool could be the base for a final stand-alone product that works without any DAW. It could be used to measure and compare plugins without any DAW. It could also be used to render audio through plugins and save the output to disk. It could also be used to listen to the output of plugins ans switch in real time. The audio files could be stored in a list that is easy switchable. Each audio file stores loop positions and can be looped. The user can define the loop positions in the GUI. 

#### Simple VST plugin that can load other plugins
- can scan for plugins and show them in a list (use standard vst installation paths)
- can load one plugin and show its parameters in a list
- can load one plugin and show its GUI in a window


### measurement part
- uses python implementation of standard algorithms (RBJ, Orfanidis, Zölzer) in a fisrt step for a python only test of the measurement tools
- measurement methods:
  - frequency response, phase response, group delay (and latency for linear phase)
  - THD, THD+N, noise, SNR
  - crosstalk
  - null test with alignment
  


## development rules
- step by step
- development of new feature
  - design
  - implementation
  - function testing 
- code review
- documentation
- readable code (no one-liners, no magic numbers, no magic constants, no magic strings)
- readability is more important than performance (but not too much)
