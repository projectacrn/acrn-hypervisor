.. _vuart_virtualization:

vUART Virtualization
###################

Architecture
************

vUART is virtual 16550 uart implemented in hypervisor. It can work as a console or a communication port. Currently, the vuart is mapped to the traditional COM port address. Uart driver in kernel can auto detect the port base and irq.

.. figure:: images/uart-virt-hld-1.png
   :align: center
   :name: uart-arch

   UART virtualization architecture

Each vUART has two FIFOs, 8192 bytes Tx FIFO and 256 bytes Rx FIFO.  Currently, we only provide 4 ports for use.

-  COM1 (port base: 0x3F8, irq: 4)

-  COM2 (port base: 0x2F8, irq: 3)

-  COM3 (port base: 0x3E8, irq: 6)

-  COM4 (port base: 0x2E8, irq: 7)

A VM can enable one console vuart and several communication vuarts.

Console vUART
*************

vUART can be used as a console port, and it can be activated by  ``vm_console <vm_id>`` command in hypervisor console. From :numref:`console-uart-arch`,  there is only one physical uart, but four console vuarts (green color blocks). A hypervisor console is implemented above the physical uart, and it works in polling mode.  There is a timer in hv console, the timer handler dispatches the input from physical uart to the vuart or the hypervisor shell process and get data from vuart’s Tx FIFO and send to physical uart. The data in vuart’s FIFOs will be overwritten when it is not taken out intime.

.. figure:: images/uart-virt-hld-2.png
   :align: center
   :name: console-uart-arch

   console vUART architecture

Communication vUART
*******************

The communication vuart is used to transfer data between two VMs in low speed. For kernel driver, it is a general uart, can be detected and probed by 8250 serial driver. But in hypervisor, it has special process.

From :numref:`communication-uart-arch`, the vuart in two VMs is connected according to the configuration in hypervisor.  When user write a byte to the communication uart in VM0:

Operations in VM0

-  VM0 uart driver put the data to THR.

-  VM trap to hypervisor, and the vuart PIO handler is called.

-  Put the data to its target vuart’s Rx FIFO.

-  Inject a Data Ready interrupt to VM1.

-  If the target vuart’s FIFO is not full, inject a THRE interrupt to VM0.

-  Return.

Operations in VM1

-  Receive an interrupt, dispatch to uart driver.

-  Read LSR register, find a Data Ready interrupt.

-  Read data from Rx FIFO.

-  If Rx FIFO is not full, inject THRE interrupt to VM0.

.. figure:: images/uart-virt-hld-3.png
   :align: center
   :name: communication-uart-arch

   communication vUART architecture

Usage
*****

-  For console vUART

   To enable the console port for a VM, only need to change the port_base and irq in ``acrn-hypervi   sor/hypervisor/scenarios/<scenario name>/vm_configurations.c``. If the irq number has been used    in your system ( ``cat /proc/interrupt``), you can choose other IRQ number. Set the .irq =0, the   vuart will work in polling mode.

   -  COM1_BASE (0x3F8) + COM1_IRQ(4)

   -  COM2_BASE (0x2F8) + COM2_IRQ(3)

   -  COM3_BASE (0x3E8) + COM3_IRQ(6)

   -  COM4_BASE (0x2E8) + COM4_IRQ(7)

   Example::

      .vuart[0] = {
                        .type = VUART_LEGACY_PIO,
                        .addr.port_base = COM1_BASE,
                        .irq = COM1_IRQ,
                  }

   The kernel bootargs ``console=ttySx`` should be the same with vuart[0], otherwise, the kernel co   nsole log can not captured by hypervisor.Then, after bringup the system, you can switch the cons   ole to the target VM by:

   .. code-block:: console
      
      ACRN:\>vm_console 0
      ----- Entering VM 0 Shell -----

-  For communication vUART
   
   To enable the communication port, you should configure vuart[1] in the two VMs which want to com   municate. The port_base and irq should not repeat with the vuart[0] in the same VM. t_vuart.vm_i   d is the target VM's vm_id, start from 0 (0 means VM0). t_vuart.vuart_id is the target vuart ind   ex in the target VM, start from 1 (1 means vuart[1]).

   Example::

      /* VM0 */
      ...
      /* VM1 */
      .vuart[1] = {
                        .type = VUART_LEGACY_PIO,
                        .addr.port_base = COM2_BASE,
                        .irq = COM2_IRQ,
                        .t_vuart.vm_id = 2U,
                        .t_vuart.vuart_id = 1U,
                        },
      ...
      /* VM2 */
      .vuart[1] = {
                        .type = VUART_LEGACY_PIO,
                        .addr.port_base = COM2_BASE,
                        .irq = COM2_IRQ,
                        .t_vuart.vm_id = 1U,
                        .t_vuart.vuart_id = 1U,
                  },

.. note:: As the device mode also has virtual uart, and also use 0x3F8 and 0x2F8 as port base. If y   ou add ``-s <slot>, lpc`` in launch script, the device model will create COM0 and COM1 for the p   ost launched VM. It will also add the port info to ACPI table. This is useful for windows, vxwor   ksas they probe driver according to ACPI table.

   If user enable both the device model uart and hypervisor vuart in the same port address, the ac    cess to the port address will be response by hypervisor vuart directly and will not pass to devi   ce model.

