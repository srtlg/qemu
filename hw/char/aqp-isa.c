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

enum aqp_state {
    State_Reset,
    // DetectHardWare
    HWD_S0,
    HWD_S1,
    HWD_S2,
    HWD_S3,
    HWD_S4,
    HWD_S5,
    HWD_S6,
    HWD_S7,
    HWD_S8,
    HWD_S9,
    HWD_S10,
    // DetectFastLink
    HWD_S11,
    HWD_S12,
    HWD_S13,
    //
    State_Error
};

enum aqp_poke_state {
    PP_RD00,
    PP_WRITE_OPCODE,
    PP_POKE_RD11,
    PP_POKE_WTAD0,
    PP_POKE_RD13,
    PP_POKE_WTAD1,
    PP_POKE_RD14,
    PP_POKE_WTAD2,
    PP_POKE_RD15,
    PP_POKE_WTAD3,

    PP_POKE_RD21,
    PP_POKE_WTV0,
    PP_POKE_RD23,
    PP_POKE_WTV1,
    PP_POKE_RD24,
    PP_POKE_WTV2,
    PP_POKE_RD25,
    PP_POKE_WTV3,

    PP_PEEK_RD11,
    PP_PEEK_WTAD0,
    PP_PEEK_RD13,
    PP_PEEK_WTAD1,
    PP_PEEK_RD14,
    PP_PEEK_WTAD2,
    PP_PEEK_RD15,
    PP_PEEK_WTAD3,

    PP_PEEK_RD21,
    PP_PEEK_RDV0,
    PP_PEEK_RD23,
    PP_PEEK_RDV1,
    PP_PEEK_RD24,
    PP_PEEK_RDV2,
    PP_PEEK_RD25,
    PP_PEEK_RDV3,

    PP_Error
};

typedef struct AQPState {
    PortioList portio_list;
    uint32_t iobase;
    enum aqp_state state;
    enum aqp_poke_state pp_state;
    uint8_t pp_opcode;
    uint32_t pp_address;
    uint32_t pp_value;
    uint8_t byte07;
    uint8_t byte10;
    uint8_t byte11;
    uint8_t byte14;
} AQPState;

typedef struct ISAAQPState {
    ISADevice parent_obj;
    uint32_t iobase;
    AQPState state;
} ISAAQPState;

#define TYPE_ISA_AQP "isa-aqp"
#define ISA_AQP(obj) \
    OBJECT_CHECK(ISAAQPState, (obj), TYPE_ISA_AQP)
/*

00  R
01 W
02  R
03  R

07 W

10 WR
11 W

14 W

*/
static Property aqp_isa_properties[] = {
    DEFINE_PROP_UINT32("iobase", ISAAQPState, iobase, 0x150),
    DEFINE_PROP_END_OF_LIST(),
};


static void aqp_reset(void *opaque)
{
    AQPState *s = opaque;
    s->state = State_Reset;
    s->pp_state = PP_RD00;
}


static uint32_t aqp_ioport_read_hw(void *opaque, uint32_t address)
{
    AQPState *s = opaque;
    uint32_t ret = 0xff;
    const uint32_t port = address - s->iobase;
#define next_pp_state(_previous, _next, _expr) \
    do { \
        assert(_previous != _next); \
        if (s->pp_state == _previous) { \
            s->pp_state = _next; \
            _expr; \
        } \
    } while (0)
    switch (port) {
    case 0x00:
        next_pp_state(PP_PEEK_RDV0, PP_PEEK_RD23, ret = 0xff & s->pp_value);
        next_pp_state(PP_PEEK_RDV1, PP_PEEK_RD24, ret = 0xff & (s->pp_value >> 8));
        next_pp_state(PP_PEEK_RDV2, PP_PEEK_RD25, ret = 0xff & (s->pp_value >> 16));
        next_pp_state(PP_PEEK_RDV3, PP_RD00,      ret = 0xff & (s->pp_value >> 24));
        if (s->state == HWD_S8 && s->pp_state == PP_RD00) { s->state = HWD_S9; }
        break;
    case 0x02:
        next_pp_state(PP_PEEK_RD21, PP_PEEK_RDV0, (void)0);
        next_pp_state(PP_PEEK_RD23, PP_PEEK_RDV1, (void)0);
        next_pp_state(PP_PEEK_RD24, PP_PEEK_RDV2, (void)0);
        next_pp_state(PP_PEEK_RD25, PP_PEEK_RDV3, (void)0);
        if (s->state == HWD_S11) { s->state = HWD_S12; }
        break;
    case 0x03:
        next_pp_state(PP_RD00, PP_WRITE_OPCODE, (void)0);

        next_pp_state(PP_POKE_RD11, PP_POKE_WTAD0, (void)0);
        next_pp_state(PP_POKE_RD13, PP_POKE_WTAD1, (void)0);
        next_pp_state(PP_POKE_RD14, PP_POKE_WTAD2, (void)0);
        next_pp_state(PP_POKE_RD15, PP_POKE_WTAD3, (void)0);

        next_pp_state(PP_POKE_RD21, PP_POKE_WTV0, (void)0);
        next_pp_state(PP_POKE_RD23, PP_POKE_WTV1, (void)0);
        next_pp_state(PP_POKE_RD24, PP_POKE_WTV2, (void)0);
        next_pp_state(PP_POKE_RD25, PP_POKE_WTV3, (void)0);

        next_pp_state(PP_PEEK_RD11, PP_PEEK_WTAD0, (void)0);
        next_pp_state(PP_PEEK_RD13, PP_PEEK_WTAD1, (void)0);
        next_pp_state(PP_PEEK_RD14, PP_PEEK_WTAD2, (void)0);
        next_pp_state(PP_PEEK_RD15, PP_PEEK_WTAD3, (void)0);
        break;
    case 0x07:
        ret = s->byte07;
        break;
    case 0x10:
        if (s->state == HWD_S7) { s->state = HWD_S8; s->byte10 = 1; }
        ret = s->byte10 | 0x01; // no error
        break;
    case 0x11:
        ret = s->byte11;
        break;
    case 0x14:
        ret = s->byte14;
        break;
    default:
        pdebug("UNHANDLED PORT %02x\n", port);
        break;
    }
    pdebug("r%02x %02x\n", port, ret);
    return ret;
}


static void aqp_ioport_write_hw(void *opaque, uint32_t address, uint32_t value)
{
#define next_state(_previous, _next, _value) \
    do { \
        assert(_previous != _next); \
        if (s->state == _previous) { \
            if (value == _value) { nextstate = _next; } \
            else { \
                s->state = State_Error; \
                pdebug("error state %i -> %i when value %i expected %i\n", _previous, _next, value, _value); \
            } \
        } \
    } while (0)
    AQPState *s = opaque;
    const uint32_t port = address - s->iobase;
    const enum aqp_state oldstate = s->state;
    enum aqp_state nextstate = s->state;
    const enum aqp_poke_state oldppstate = s->pp_state;
    switch (port) {
    case 0x01:
        if (s->pp_state == PP_WRITE_OPCODE) {
            switch (value) {
            case 0:
                s->pp_state = PP_POKE_RD11;
                break;
            case 1:
                s->pp_state = PP_PEEK_RD11;
                break;
            default:
                pdebug("unknown opcode for ioctl %x\n", value);
                assert(0);
                break;
            }
        }
        next_pp_state(PP_POKE_WTAD0, PP_POKE_RD13, s->pp_address = 0xff & value);
        next_pp_state(PP_POKE_WTAD1, PP_POKE_RD14, s->pp_address |= ((0xff & value) << 8));
        next_pp_state(PP_POKE_WTAD2, PP_POKE_RD15, s->pp_address |= ((0xff & value) << 16));
        next_pp_state(PP_POKE_WTAD3, PP_POKE_RD21, s->pp_address |= ((0xff & value) << 24));

        next_pp_state(PP_POKE_WTV0, PP_POKE_RD23, s->pp_value = 0xff & value);
        next_pp_state(PP_POKE_WTV1, PP_POKE_RD24, s->pp_value |= (0xff & value) << 8);
        next_pp_state(PP_POKE_WTV2, PP_POKE_RD25, s->pp_value |= (0xff & value) << 16);
        next_pp_state(PP_POKE_WTV3, PP_RD00,      s->pp_value |= (0xff & value) << 24);

        next_pp_state(PP_PEEK_WTAD0, PP_PEEK_RD13, s->pp_address = 0xff & value);
        next_pp_state(PP_PEEK_WTAD1, PP_PEEK_RD14, s->pp_address |= ((0xff & value) << 8));
        next_pp_state(PP_PEEK_WTAD2, PP_PEEK_RD15, s->pp_address |= ((0xff & value) << 16));
        next_pp_state(PP_PEEK_WTAD3, PP_PEEK_RD21, s->pp_address |= ((0xff & value) << 24));

        if (oldppstate == PP_POKE_WTV3 && s->pp_state == PP_RD00) { pdebug("POKE(%x, %x)\n", s->pp_address, s->pp_value); }
        if (oldppstate == PP_PEEK_WTAD3 && s->pp_state == PP_PEEK_RD21) { pdebug("PEEK(%x) => %x\n", s->pp_address, s->pp_value); }
        if (s->state == HWD_S6 && oldppstate == PP_POKE_WTV3 && s->pp_state == PP_RD00) { nextstate = HWD_S7; s->pp_value = 0x1234567; }
        break;
    case 0x07:
        next_state(HWD_S1, HWD_S2, 8);
        next_state(HWD_S2, HWD_S3, 0);
        next_state(HWD_S10, HWD_S11, 1);
        next_state(HWD_S12, HWD_S13, 0);
        s->byte07 = value;
        break;
    case 0x10:
        next_state(HWD_S0, HWD_S1, 0);
        next_state(HWD_S4, HWD_S5, 1);
        next_state(HWD_S5, HWD_S6, 0);
        s->byte10 = value;
        break;
    case 0x11:
        next_state(State_Reset, HWD_S0, 0);
        s->byte11 = value;
        break;
    case 0x14:
        next_state(HWD_S3, HWD_S4, 0x80);
        next_state(HWD_S9, HWD_S10, 0);
        s->byte14 = value;
        break;
    default:
        break;
    }
    s->state = nextstate;
    pdebug("w%02x %02x      (%d->%d | %d->%d)\n", port, value, oldstate, s->state, oldppstate, s->pp_state);
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
    s->iobase = base;
    qemu_register_reset(aqp_reset, s);
    isa_register_portio_list(isadev, &s->portio_list, base, &aqp_isa_portio_list[0], s, "aqp");
}


static const VMStateDescription vmstate_aqp_isa = {
    .name = "aqp_isa",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(state.byte07, ISAAQPState),
        VMSTATE_UINT8(state.byte10, ISAAQPState),
        VMSTATE_UINT8(state.byte11, ISAAQPState),
        VMSTATE_UINT8(state.byte14, ISAAQPState),
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
