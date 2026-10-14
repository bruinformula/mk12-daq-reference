# BFR Mk. 12 CAN Bootloader: Developer Guide

Complete guide for Bruin Formula Racing engineers on how to flash the CAN bootloader to fresh boards and how to flash application firmware over the vehicle CAN bus using both **`bfr-cli`** and **`STM32CubeIDE`**.

---

## 1. System Architecture Overview

* **Microcontroller**: STM32U535CCT6 (ARM Cortex-M33, 256 KB Flash, 272 KB SRAM)
* **CAN Hardware**: Bosch M_CAN (FDCAN1)
  * **RX Pin**: `PB8` (Alternate Function AF9)
  * **TX Pin**: `PB9` (Alternate Function AF9)
* **Status LED (D18)**: `PB7` (Active HIGH, drives base of `Q4` MMBT3904)
  * **5 Hz Blink**: Idling in Bootloader Mode (waiting for commands)
  * **Rapid Toggle (20 Hz)**: Actively writing quad-words to flash
  * **OFF**: Running Application Firmware
* **Dual-Mode Protocol**:
  * **CAN-FD Mode**: 48 bytes payload per frame, 2 Mbps bit-rate switching (BRS) — flashes 224 KB in ~1.5 seconds.
  * **Classic CAN 2.0B Mode**: 96-byte streaming blocks with Block-ACK — flashes in ~4.5 seconds.

### Memory Map

| Partition | Start Address | End Address | Size | Erased Over CAN? | Purpose |
| :--- | :--- | :--- | :--- | :---: | :--- |
| **Bootloader** | `0x08000000` | `0x08007FFF` | 32 KB (Pages 0–3) | **NO (Protected)** | Permanent bootloader firmware |
| **Application** | `0x08008000` | `0x0803FFFF` | 224 KB (Pages 4–31) | **YES** | Board user code (SDU, MDU, etc.) |
| **RAM Magic** | `0x20000000` | `0x20000003` | 4 Bytes | N/A | Bootloader entry token (`0xDEADBEEF`) |

---

## 2. Flashing the Bootloader to a Fresh Board (One-Time Setup)

Every new STM32U535 board needs the bootloader flashed **once** via ST-Link on the bench before installation into the chassis.

```mermaid
graph LR
    FreshBoard[Unprogrammed Board] --> STLink[Connect ST-Link SWD]
    STLink --> FlashBL[Flash bootloader.bin to 0x08000000]
    FlashBL --> BenchTest[Verify PB7 Red LED blinks at 5 Hz]
    BenchTest --> InstallChassis[Install Board in Vehicle]
```

### Method A: Via Command Line (`bfr-cli`)

1. Connect your ST-Link V2/V3 to the board SWD header (`SWDIO`, `SWCLK`, `GND`, `3V3`).
2. Run the single `bfr` bootloader flash command:
   ```bash
   bfr flash-bootloader
   # or: bfr flash bootloader
   ```
   **What `bfr` does automatically:**
   * Locates `mk12-daq-reference/Bootloader`
   * Compiles the fresh `bootloader.bin` using the native ARM GCC toolchain
   * Discovers your local `STM32_Programmer_CLI` installation
   * Flashes the binary directly to base address `0x08000000` with verification (`-v`) and system reset (`-rst`)
3. **Verification**: Once flashed, the red status LED `D18` on `PB7` will immediately begin blinking at **5 Hz**. This confirms the bootloader is running and waiting for CAN commands.

> [!TIP]
> You can also run the raw STM32CubeProgrammer CLI directly if preferred:
> ```bash
> STM32_Programmer_CLI -c port=SWD -w ~/mk12-daq-reference/Bootloader/build/bootloader.bin 0x08000000 -v -rst
> ```

---

### Method B: Via STM32CubeIDE / STM32CubeProgrammer GUI

1. Open **STM32CubeProgrammer** (or STM32CubeIDE's built-in target programmer).
2. Connect your ST-Link to your laptop and click **Connect**.
3. Under the **Erasing & Programming** tab:
   * **File Path**: Browse to `~/mk12-daq-reference/Bootloader/build/bootloader.bin` (or `bootloader.elf`).
   * **Start Address**: `0x08000000`.
   * Check **Verify programming** and **Run after programming**.
4. Click **Start Programming**.
5. Disconnect the ST-Link. The red LED on `PB7` will blink at 5 Hz.

---

## 3. Configuring Your Board Application Project

To allow an application (e.g. SDU, MDU, SMU) to coexist with the bootloader, three small changes are required in the board's project repository.

### Step 3.1: Linker Script Flash Origin (`0x08008000`)

Open the board project's linker script (e.g. `STM32U535xxxx_FLASH.ld`):

```diff
 MEMORY
 {
   RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 272K
-  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
+  FLASH  (rx)  : ORIGIN = 0x08008000, LENGTH = 224K
 }
```

### Step 3.2: Relocate Vector Table (VTOR)

In your application's `Core/Src/main.c`, right inside `main()` before initializing peripherals:

```c
int main(void)
{
    /* Relocate Vector Table to Application Partition */
    SCB->VTOR = 0x08008000UL;

    HAL_Init();
    SystemClock_Config();
    // ...
```

### Step 3.3: In-Application CAN Bootloader Hook

Include the reference hook from `mk12-daq-reference`:

1. In `Core/Src/main.c`:
   ```c
   #include "bootloader_client.h"

   int main(void)
   {
       SCB->VTOR = 0x08008000UL;
       HAL_Init();
       SystemClock_Config();
       MX_GPIO_Init();
       MX_FDCAN1_Init();

       /* Initialize Bootloader Hook (Example for SDU) */
       BootloaderClient_Init(BOARD_ID_SDU, CAN_ID_TARGET_SDU, CAN_ID_RESP_SDU);

       while (1) {
           // User application loop
       }
   }
   ```

2. In your CAN reception callback (`Core/Src/main.c` or `Core/Src/fdcan.c`):
   ```c
   void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
   {
       FDCAN_RxHeaderTypeDef rx_header;
       uint8_t rx_data[64];

       if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
           /* Check if message is a bootloader trigger */
           if (BootloaderClient_ProcessFDCAN(hfdcan, &rx_header, rx_data)) {
               return; /* Handled by bootloader client; MCU will reset into bootloader */
           }

           /* Normal application CAN frame handling continues below... */
       }
   }
   ```

---

## 4. Flashing Over CAN Using `bfr-cli`

With the car assembled and boards sealed in their enclosures, plug your USB CAN adapter (CANable, CANdapter, PCAN) into the central vehicle diagnostic port.

```
[Laptop] ---> USB ---> [CANable / CANdapter] ---> Diagnostic Port (CAN_H / CAN_L) ---> [Vehicle Bus]
```

### 1. Discover Active Boards
Run `bfr can-scan` to verify all boards are alive on the bus:
```bash
bfr can-scan
```
**Example Output:**
```
>>> Scanning BFR Mk. 12 DAQ Network over CAN...

Board    Name                                   Target ID   Resp ID    Mode                       Firmware  
---------------------------------------------------------------------------------------------------------
SDU      SDU (Suspension Data Unit)             0x014       0x015      APPLICATION                v1.0.0
MDU      MDU (Master Data Unit)                 0x010       0x011      APPLICATION                v1.0.0
DVCU     DVCU (DAQ Vehicle Control Unit)        0x012       0x013      BOOTLOADER                 v1.0.0
SMU      SMU (Spatial Monitoring Unit / IMU)    0x016       0x017      APPLICATION                v1.0.0
SKU      SKU (Suspension & Kinematics Unit)     0x018       0x019      Offline / No Response
PMU      PMU (Pneumatic Measurement Unit)       0x01A       0x01B      APPLICATION                v1.0.0
TSMU     TSMU (Tractive System Measurement)     0x01C       0x01D      APPLICATION                v1.0.0
SGU      SGU (Strain Gauge Unit)                0x01E       0x01F      APPLICATION                v1.0.0
---------------------------------------------------------------------------------------------------------

Summary: 6 of 8 DAQ nodes responding.
```

### 2. Flash a Board
To flash pre-compiled firmware:
```bash
# Auto-detects ELF in Debug/ and converts to BIN:
bfr can-flash sdu

# Or specify a specific .bin or .elf file:
bfr can-flash sdu ~/mk12-sdu/Debug/mk12-sdu.elf

# Force classic CAN 2.0 (8-byte frames):
bfr can-flash sdu --classic

# Specify explicit serial port if multiple adapters are connected:
bfr can-flash sdu --channel /dev/cu.usbmodem1101
```

### 3. Deploy in One Step
To clean, compile, and flash over CAN in one shot:
```bash
bfr deploy sdu --can
```

---

## 5. Flashing Over CAN Inside STM32CubeIDE

You can configure STM32CubeIDE so developers can flash over CAN with a single click or keyboard shortcut directly from the IDE.

### Method 1: External Tool Shortcut (One-Click CAN Flash)

1. In STM32CubeIDE, open the top menu: **Run** $\rightarrow$ **External Tools** $\rightarrow$ **External Tools Configurations...**
2. Double-click **Program** on the left to create a new configuration.
3. Configure the **Main** tab:
   * **Name**: `BFR CAN Flash (${project_name})`
   * **Location**: `/Users/larry/bfr-cli/bfr` (or path to your `bfr` executable)
   * **Working Directory**: `${project_loc}`
   * **Arguments**:
     ```text
     can-flash ${project_name} "${project_loc}/Debug/${project_name}.elf"
     ```
4. Configure the **Build** tab:
   * Check **Build before launching**.
   * Select **The project containing the selected resource**.
5. Click **Apply**, then **Run**.

> [!TIP]
> Once saved, this tool appears under the **External Tools** toolbar button (the red play icon with a briefcase). You can assign a keyboard shortcut to it under **Preferences** $\rightarrow$ **General** $\rightarrow$ **Keys** (search for *External Tools*).

---

### Method 2: Automatic Post-Build Step

If you want every successful build in STM32CubeIDE to automatically prompt or trigger a CAN flash:

1. Right-click your project $\rightarrow$ **Properties**.
2. Navigate to **C/C++ Build** $\rightarrow$ **Settings** $\rightarrow$ **Build Steps** tab.
3. In the **Post-build steps** box:
   * **Command**:
     ```bash
     /Users/larry/bfr-cli/bfr can-flash ${ProjName} "${BuildArtifactFileName}"
     ```
   * **Description**: `Flashing over CAN bus via bfr-cli`
4. Click **Apply and Close**.

---

### Method 3: Bench Debugging via ST-Link (Still 100% Functional!)

Whenever a board is on the test bench, **normal ST-Link debugging works without changes**:

1. Because the linker script places your application at `0x08008000`, when you click **Debug** or **Run** in STM32CubeIDE with an ST-Link connected:
   * ST-Link writes directly to `0x08008000`.
   * **The CAN bootloader at `0x08000000` is untouched.**
2. Breakpoints, live variable inspection, and step-through debugging work identically as before.

---

## 6. CAN ID Reference Table (Mk. 12 DAQ Bus)

| Board Name | Key | Acronym | Target CAN ID (Host $\rightarrow$ Node) | Response CAN ID (Node $\rightarrow$ Host) | Board ID |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Global Broadcast** | `all` | `ALL` | `0x07F` | N/A | `0xFF` |
| **Master Data Unit** | `mdu` | `MDU` | `0x010` | `0x011` | `0x01` |
| **DAQ Vehicle Control Unit** | `dvcu` | `DVCU` | `0x012` | `0x013` | `0x02` |
| **Suspension Data Unit** | `sdu` | `SDU` | `0x014` | `0x015` | `0x03` |
| **Spatial Monitoring Unit** | `smu` | `SMU` | `0x016` | `0x017` | `0x04` |
| **Suspension & Kinematics** | `sku` | `SKU` | `0x018` | `0x019` | `0x05` |
| **Pneumatic Measurement** | `pmu` | `PMU` | `0x01A` | `0x01B` | `0x06` |
| **Tractive System Measurement** | `tsmu` | `TSMU` | `0x01C` | `0x01D` | `0x07` |
| **Strain Gauge Unit** | `sgu` | `SGU` | `0x01E` | `0x01F` | `0x08` |

---

## 7. Failsafe & Recovery Guide

### Power Loss During Flashing
* If power is cut mid-flash, the application partition (`0x08008000`) will have an incomplete image.
* On the next power-up, the bootloader's integrity check (`Bootloader_CheckAppValid()`) detects that the vector table is invalid.
* **Result**: The MCU **remains in bootloader mode permanently** (red LED blinking at 5 Hz). The device is **unbrickable** over CAN.
* **Recovery**: Simply run `bfr can-flash <board>` again to rewrite the image.

### Full Chip Erase Recovery
* If someone accidentally performs a full chip erase via ST-Link, re-flash `bootloader.bin` to `0x08000000` using Step 2 above.
