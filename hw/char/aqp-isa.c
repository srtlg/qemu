/*
 * Bruker ISA AQP Card emulation
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "chardev/char-parallel.h"
#include "chardev/char-fe.h"
#include "hw/isa/isa.h"
#include "sysemu/sysemu.h"
#include "trace.h"

#define DEBUG_AQP

#ifdef DEBUG_AQP
#define pdebug(fmt, ...) printf("aqp: " fmt, ## __VA_ARGS__)
#else
#define pdebug(fmt, ...) ((void)0)
#endif

typedef struct AQPState {
    PortioList portio_list;
    uint8_t byte7;
} AQPState;

typedef struct ISAAQPState {
    ISADevice parent_obj;
    uint32_t iobase;
    AQPState state;
} ISAAQPState;

#define TYPE_ISA_AQP "isa-aqp"
#define ISA_AQP(obj) \
    OBJECT_CHECK(ISAAQPState, (obj), TYPE_ISA_AQP)


static Property aqp_isa_properties[] = {
    DEFINE_PROP_UINT32("iobase", ISAAQPState, iobase, 0x150),
    DEFINE_PROP_END_OF_LIST(),
};


static void aqp_reset(void *opaque)
{
    AQPState *s = opaque;
    (void)s;
}


static uint32_t aqp_ioport_read_hw(void *opaque, uint32_t address)
{
    AQPState *s = opaque;
    uint32_t ret = 0xff;

    (void)s;
    pdebug("r%02x %02x\n", address, ret);
    return ret;
}


static void aqp_ioport_write_hw(void *opaque, uint32_t address, uint32_t value)
{
    AQPState *s = opaque;
    (void)s;
    pdebug("w%02x %02x\n", address, value);
}


static const MemoryRegionPortio aqp_isa_portio_list[] = {
    {
        .offset = 0, .len = 32, .size = 1,
        .read = aqp_ioport_read_hw,
        .write = aqp_ioport_write_hw },
    PORTIO_END_OF_LIST(),
};


static void aqp_isa_realizefn(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    ISAAQPState *isa = ISA_AQP(dev);
    AQPState *s = &isa->state;
    uint32_t base;

    base = isa->iobase;
    qemu_register_reset(aqp_reset, s);
    isa_register_portio_list(isadev, &s->portio_list, base, &aqp_isa_portio_list[0], s, "aqp");
}


static const VMStateDescription vmstate_aqp_isa = {
    .name = "aqp_isa",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(state.byte7, ISAAQPState),
        VMSTATE_END_OF_LIST()
    }
};

static void aqp_isa_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = aqp_isa_realizefn;
    dc->vmsd = &vmstate_aqp_isa;
    dc->props = aqp_isa_properties;
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
}


static const TypeInfo aqp_isa_info = {
    .name          = TYPE_ISA_AQP,
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(ISAAQPState),
    .class_init    = aqp_isa_class_initfn,
};

static void aqp_register_types(void)
{
    type_register_static(&aqp_isa_info);
}

type_init(aqp_register_types)

// vim: set sws=4 et sw=4 ts=4:
