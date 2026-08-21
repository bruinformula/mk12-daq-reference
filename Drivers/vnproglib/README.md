## VectorNav STM32 Setup
This is a stripped-down version of the `vnproglib` which should be ready to add to STM32 with the following steps:

1. Copy the `vn/` folder inside `include/` into `Core/Inc/`
2. Copy the `vn/` folder inside `src/` into `Core/Src/`

After copying, your project folders should look like:
1. `Core/Inc/vn/`
2. `Core/Src/vn/`

### Code Notes:
- Make sure to use `VnCompositeData` to organize sensor data.
- If you process UART data inside the interrupt, wrap the data reads with `__disable_irq()` / `__enable_irq()` for safe access (so the IRQ doesn't access data at the same time as the read). If you process in `main()` you don't need these function calls
- Might have to change `VNUART_PROTOCOL_BUFFER_SIZE` depending on how much data this specific VN-300 Rugged transmits at once
- Match baud rate, use DMA (circular mode)