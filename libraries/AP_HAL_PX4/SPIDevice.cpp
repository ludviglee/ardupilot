/*
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "SPIDevice.h"

#include <arch/board/board.h>
#include "board_config.h"
#include <drivers/device/spi.h>
#include <stdio.h>
#include <AP_HAL/AP_HAL.h>
#include <AP_HAL/utility/OwnPtr.h>
#include "Scheduler.h"
#include "Semaphores.h"

namespace PX4 {

#define MHZ (1000U*1000U)
#define KHZ (1000U)

SPIDesc SPIDeviceManager::device_table[] = {
    SPIDesc("mpu6000",      PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_MPU, SPIDEV_MODE3, 500*KHZ, 11*MHZ),
#if defined(PX4_SPIDEV_EXT_BARO)
    SPIDesc("ms5611_ext",   PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT_BARO, SPIDEV_MODE3, 20*MHZ, 20*MHZ),
#endif
#if defined(PX4_SPIDEV_ICM)
    SPIDesc("icm20608",   PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_ICM, SPIDEV_MODE3, 20*MHZ, 20*MHZ),
#endif
    // ICM20608 on the ACCEL_MAG 
    SPIDesc("icm20608-am",   PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_ACCEL_MAG, SPIDEV_MODE3, 20*MHZ, 20*MHZ),
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_PX4_V4
    SPIDesc("ms5611_int",   PX4_SPI_BUS_BARO, (spi_dev_e)PX4_SPIDEV_BARO, SPIDEV_MODE3, 20*MHZ, 20*MHZ),
#endif
#ifdef PX4_SPIDEV_BARO
    SPIDesc("ms5611",       PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_BARO, SPIDEV_MODE3, 20*MHZ, 20*MHZ),
#endif
    SPIDesc("lsm9ds0_am",   PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_ACCEL_MAG, SPIDEV_MODE3, 11*MHZ, 11*MHZ),
#ifdef PX4_SPIDEV_EXT_ACCEL_MAG
    SPIDesc("lsm9ds0_ext_am", PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT_ACCEL_MAG, SPIDEV_MODE3, 11*MHZ, 11*MHZ),
#endif
    SPIDesc("lsm9ds0_g",    PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_GYRO, SPIDEV_MODE3, 11*MHZ, 11*MHZ),
#ifdef PX4_SPIDEV_EXT_GYRO
    SPIDesc("lsm9ds0_ext_g",PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT_GYRO, SPIDEV_MODE3, 11*MHZ, 11*MHZ),
#endif
    SPIDesc("mpu9250",      PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_MPU, SPIDEV_MODE3, 1*MHZ, 11*MHZ),
#ifdef PX4_SPIDEV_EXT_MPU
    SPIDesc("mpu6000_ext",  PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT_MPU, SPIDEV_MODE3, 500*KHZ, 11*MHZ),    
    SPIDesc("mpu9250_ext",  PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT_MPU, SPIDEV_MODE3, 1*MHZ, 11*MHZ),
    SPIDesc("icm20608_ext", PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT_MPU, SPIDEV_MODE3, 1*MHZ, 11*MHZ),
#endif
#ifdef PX4_SPIDEV_HMC
    SPIDesc("hmc5843",      PX4_SPI_BUS_SENSORS, (spi_dev_e)PX4_SPIDEV_HMC, SPIDEV_MODE3, 11*MHZ, 11*MHZ),
#endif

#ifdef PX4_SPI_BUS_EXT
#ifdef PX4_SPIDEV_EXT0
    SPIDesc("external0",    PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT0, SPIDEV_MODE3, 5*MHZ, 5*MHZ),
#endif
#ifdef PX4_SPIDEV_EXT1
    SPIDesc("external1",    PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT1, SPIDEV_MODE3, 5*MHZ, 5*MHZ),
#endif
#ifdef PX4_SPIDEV_EXT2
    SPIDesc("external2",    PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT2, SPIDEV_MODE3, 5*MHZ, 5*MHZ),
#endif
#ifdef PX4_SPIDEV_EXT3
    SPIDesc("external3",    PX4_SPI_BUS_EXT, (spi_dev_e)PX4_SPIDEV_EXT3, SPIDEV_MODE3, 5*MHZ, 5*MHZ),
#endif
#endif

    SPIDesc(nullptr, 0, (spi_dev_e)0, (spi_mode_e)0, 0, 0),
};

SPIDevice::SPIDevice(SPIBus &_bus, SPIDesc &_device_desc)
    : bus(_bus)
    , device_desc(_device_desc)
{
    set_device_bus(_bus.bus);
    set_device_address(_device_desc.device);
    set_speed(AP_HAL::Device::SPEED_LOW);
    SPI_SELECT(bus.dev, device_desc.device, false);
    asprintf(&pname, "SPI:%s:%u:%u",
             device_desc.name,
             (unsigned)bus.bus, (unsigned)device_desc.device);
    perf = perf_alloc(PC_ELAPSED, pname);
    printf("SPI device %s on %u:%u at speed %u mode %u\n",
           device_desc.name,
           (unsigned)bus.bus, (unsigned)device_desc.device,
           (unsigned)frequency, (unsigned)device_desc.mode);
}

SPIDevice::~SPIDevice()
{
    printf("SPI device %s on %u:%u closed\n", device_desc.name,
           (unsigned)bus.bus, (unsigned)device_desc.device);
    perf_free(perf);
    free(pname);
}

bool SPIDevice::set_speed(AP_HAL::Device::Speed speed)
{
    switch (speed) {
    case AP_HAL::Device::SPEED_HIGH:
        frequency = device_desc.highspeed;
        break;
    case AP_HAL::Device::SPEED_LOW:
        frequency = device_desc.lowspeed;
        break;
    }
    return true;
}

/*
  low level transfer function
 */
void SPIDevice::do_transfer(uint8_t *send, uint8_t *recv, uint32_t len)
{
    irqstate_t state = irqsave();
    perf_begin(perf);
    SPI_SETFREQUENCY(bus.dev, frequency);
    SPI_SETMODE(bus.dev, device_desc.mode);
    SPI_SETBITS(bus.dev, 8);
    SPI_SELECT(bus.dev, device_desc.device, true);
    SPI_EXCHANGE(bus.dev, send, recv, len);
    SPI_SELECT(bus.dev, device_desc.device, false);
    perf_end(perf);
    irqrestore(state);
}

bool SPIDevice::transfer(const uint8_t *send, uint32_t send_len,
                         uint8_t *recv, uint32_t recv_len)
{
    uint8_t buf[send_len+recv_len];
    if (send_len > 0) {
        memcpy(buf, send, send_len);
    }
    if (recv_len > 0) {
        memset(&buf[send_len], 0, recv_len);
    }
    do_transfer(buf, buf, send_len+recv_len);
    if (recv_len > 0) {
        memcpy(recv, &buf[send_len], recv_len);
    }
    return true;
}

bool SPIDevice::transfer_fullduplex(const uint8_t *send, uint8_t *recv, uint32_t len)
{
    uint8_t buf[len];
    memcpy(buf, send, len);
    do_transfer(buf, buf, len);
    memcpy(recv, buf, len);
    return true;
}

AP_HAL::Semaphore *SPIDevice::get_semaphore()
{
    return &bus.semaphore;
}

    
AP_HAL::Device::PeriodicHandle SPIDevice::register_periodic_callback(uint32_t period_usec, AP_HAL::Device::PeriodicCb cb)
{
    return bus.register_periodic_callback(period_usec, cb);
}

bool SPIDevice::adjust_periodic_callback(AP_HAL::Device::PeriodicHandle h, uint32_t period_usec)
{
    return false;
}


/*
  return a SPIDevice given a string device name
 */    
AP_HAL::OwnPtr<AP_HAL::SPIDevice>
SPIDeviceManager::get_device(const char *name)
{
    /* Find the bus description in the table */
    uint8_t i;
    for (i = 0; device_table[i].name; i++) {
        if (strcmp(device_table[i].name, name) == 0) {
            break;
        }
    }
    if (device_table[i].name == nullptr) {
        printf("SPI: Invalid device name: %s\n", name);
        return AP_HAL::OwnPtr<AP_HAL::SPIDevice>(nullptr);
    }

    SPIDesc &desc = device_table[i];

    // find the bus
    SPIBus *busp;
    for (busp = buses; busp; busp = (SPIBus *)busp->next) {
        if (busp->bus == desc.bus) {
            break;
        }
    }
    if (busp == nullptr) {
        // create a new one
        busp = new SPIBus;
        if (busp == nullptr) {
            return nullptr;
        }
        busp->next = buses;
        busp->bus = desc.bus;
		busp->dev = up_spiinitialize(desc.bus);
        buses = busp;
    }

    return AP_HAL::OwnPtr<AP_HAL::SPIDevice>(new SPIDevice(*busp, desc));
}

}
