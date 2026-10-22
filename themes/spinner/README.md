The spinner and BGRT themes share this image directory.

Two-step optionally loads `secure-boot-enabled.png` or
`secure-boot-disabled.png` according to the firmware Secure Boot state.
Either image can be omitted independently to hide that state's emblem.
The SVG files are editable sources for the shipped 40 × 40 PNG images.

During boot, the emblem fades in over 0.2 seconds, stays visible for one
second, and fades out over 0.5 seconds. It also remains visible while a
password prompt is displayed. Other splash modes show it only during
password prompts.

`SecureBootHorizontalAlignment` and `SecureBootVerticalAlignment` in the
theme's `[two-step]` section position the emblem; their defaults are 0.9833
and 0.97, placing it at the bottom right with 1.67% horizontal and 3%
vertical margins. The kernel argument `secure_boot.warn_if_disabled=false` suppresses
the disabled emblem.
