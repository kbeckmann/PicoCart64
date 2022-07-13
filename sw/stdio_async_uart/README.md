# stdio_async_uart

This is a simple interrupt based asynchronous UART TX module that works as a drop-in replacement for stdio_uart.

It uses a ringbuffer to queue up data that is to be transmitted, and starts by sending one byte.

When the byte has been transmitted, an interrupt is fired, and the next one is sent, until the tail of the ringbuffer has caught up with the head.

If you want to use this in your own project, make sure to copy ringbuf.h as well.