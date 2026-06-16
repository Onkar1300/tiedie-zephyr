# DFU Package Creation and Flashing Instructions

## Step 1: Create `dfu_package.zip`

Generate the DFU package using the following command (In following command, 50ms advertisement interval was used):

```bash
nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application 50/zephyr.hex --application-version 1 dfu_package.zip
```

## Step 2: Prepare NRF Dongles

Put your NRF dongles into reset mode by pressing the button once. The LED on the dongle should start blinking red.

## Step 3: Flash All Connected NRFs

Flash the `dfu_package.zip` onto all connected NRF dongles that are in reset mode:

```bash
nrfutil.exe device program --firmware dfu_package.zip --traits nordicDfu
```

After approximately 8 seconds, the LEDs on the dongles should turn back to green, indicating a successful flash. If the flashing process fails, retrying once or twice usually resolves the issue.

Note: To install nrfutil, follow steps at https://docs.nordicsemi.com/r/bundle/nrfutil/page/get-started