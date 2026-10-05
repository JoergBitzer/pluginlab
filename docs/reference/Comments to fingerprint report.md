Comments to fingerprint report

1) Impulse signal should be slightly delayed to get non-causal behaviour implemented by a delay. 
2) add L unequal R as a test signal
3) Is it possible to show the exact channel layout a plugin is made for?
4) difference report. report the absolute difference as well as the relative difference. Relative (is RMS(a) + RMS(b) a better denominator?) I am not sure? Argue with me!
5) All decision thresholds should be stored in a settings.json file in .config
6) exchange 2147748364 as steps with the text continoous as suggested
7) delete the analyses frequency for different sampling rate. Keep the latency analysis. The first assume LTI. This is part of the analyzer yet to come. Everything hinting to EQ as an effect should be deleted here.
8) The analysis in other are important for a developer and should be more upfront. All single yes /no single value results should be in a Table (selected algorithms as entries) in the plugin and additionally in the report file.
9) Blocksize dependicies: Several BL Algorithms like BL gain show different values for different blocksize. All very small but noticable. For a simple gain this should not happen, except there is a smoothing of the parameter. What happens if you render a full second through the plugin an measure the diff only at the last 100ms? a second should be enough to adapt to the changes. Many algorithms change their parameter at each new block. Therefore, the blocksize has a significant influence of the parameter behaviour. A good algorithm is independent. 