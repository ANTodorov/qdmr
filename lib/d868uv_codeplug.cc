#include "d868uv_codeplug.hh"
#include "config.hh"
#include "utils.hh"
#include "channel.hh"
#include "gpssystem.hh"
#include "userdatabase.hh"
#include "config.h"
#include "logger.hh"
#include "utils.hh"
#include <cmath>

#include <QTimeZone>
#include <QtEndian>
#include <QSet>


#define NUM_CHANNELS              4000
#define NUM_CHANNEL_BANKS         32
#define CHANNEL_BANK_0            0x00800000
#define CHANNEL_BANK_SIZE         0x00002000
#define CHANNEL_BANK_31           0x00fc0000
#define CHANNEL_BANK_31_SIZE      0x00000800
#define CHANNEL_BANK_OFFSET       0x00040000
#define CHANNEL_BITMAP            0x024c1500
#define CHANNEL_BITMAP_SIZE       0x00000200
#define CHANNEL_SIZE              0x00000040

#define VFO_A_ADDR                0x00fc0800 // Address of VFO A settings (channel_t)
#define VFO_B_ADDR                0x00fc0840 // Address of VFO B settings (channel_t)
#define VFO_SIZE                  0x00000040 // Size of each VFO settings.

#define NUM_CONTACTS              10000      // Total number of contacts
#define NUM_CONTACT_BANKS         2500       // Number of contact banks
#define CONTACTS_PER_BANK         4
#define CONTACT_BANK_0            0x02680000 // First bank of 4 contacts
#define CONTACT_BANK_SIZE         0x00000190 // Size of 4 contacts
#define CONTACT_INDEX_LIST        0x02600000 // Address of contact index list
#define CONTACTS_BITMAP           0x02640000 // Address of contact bitmap
#define CONTACTS_BITMAP_SIZE      0x00000500 // Size of contact bitmap
#define CONTACT_SIZE              0x00000064 // Size of contact element
#define CONTACT_ID_MAP            0x04340000 // Address of ID->Contact index map
#define CONTACT_ID_ENTRY_SIZE     0x00000008 // Size of each map entry


#define NUM_ANALOGCONTACTS        128
#define NUM_ANALOGCONTACT_BANKS   64
#define ANALOGCONTACTS_PER_BANK   2
#define ANALOGCONTACT_BANK_0      0x02940000
#define ANALOGCONTACT_BANK_SIZE   0x00000030
#define ANALOGCONTACT_INDEX_LIST  0x02900000 // Address of analog contact index list
#define ANALOGCONTACT_LIST_SIZE   0x00000080 // Size of analog contact index list
#define ANALOGCONTACT_BYTEMAP     0x02900100 // Address of contact bytemap
#define ANALOGCONTACT_BYTEMAP_SIZE 0x00000080 // Size of contact bytemap
#define ANALOGCONTACT_SIZE        0x00000018 // Size of analog contact
static_assert(
  ANALOGCONTACT_SIZE == sizeof(D868UVCodeplug::analog_contact_t),
  "D868UVCodeplug::analog_contact_t size check failed.");

#define NUM_RXGRP                 250        // Total number of RX group lists
#define ADDR_RXGRP_0              0x02980000 // Address of the first RX group list.
#define RXGRP_SIZE                0x00000120 // Size of each RX group list.
#define RXGRP_OFFSET              0x00000200 // Offset between group lists.
#define RXGRP_BITMAP              0x025C0B10 // Address of RX group list bitmap.
#define RXGRP_BITMAP_SIZE         0x00000020 // Storage size of RX group list bitmap.
static_assert(
  RXGRP_SIZE == sizeof(D868UVCodeplug::grouplist_t),
  "D868UVCodeplug::grouplist_t size check failed.");

#define NUM_ZONES                 250        // Maximum number of zones
#define NUM_CH_PER_ZONE           250        // Maximum number of channels per zone
#define ADDR_ZONE                 0x01000000 // Address of zone-channel lists, see zone_t
#define ZONE_SIZE                 0x00000200 // Size of each zone-channel list
#define ZONE_OFFSET               0x00000200 // Offset between zone-channel lists
#define ADDR_ZONE_NAME            0x02540000 // Address of zone names.
#define ZONE_NAME_SIZE            0x00000010 // Size of zone names
#define ZONE_NAME_OFFSET          0x00000020 // Offset between zone names.
#define ZONE_BITMAPS              0x024c1300 // Bitmap of all enabled zones
#define ZONE_BITMAPS_SIZE         0x00000020 // Size of the zone bitmap

#define NUM_RADIOIDS              250
#define ADDR_RADIOIDS             0x02580000
#define RADIOID_SIZE              0x00000020
#define RADIOID_BITMAP            0x024c1320
#define RADIOID_BITMAP_SIZE       0x00000020
static_assert(
  RADIOID_SIZE == sizeof(D868UVCodeplug::radioid_t),
  "D868UVCodeplug::radioid_t size check failed.");

#define NUM_SCAN_LISTS            250
#define NUM_SCANLISTS_PER_BANK    16
#define SCAN_LIST_BANK_0          0x01080000 // First scanlist bank
#define SCAN_LIST_OFFSET          0x00000200 // Offset to next list.
#define SCAN_LIST_SIZE            0x00000090 // Size of scan-list.
#define SCAN_LIST_BANK_OFFSET     0x00040000 // Offset to next bank
#define SCAN_BITMAP               0x024c1340 // Address of scan-list bitmap.
#define SCAN_BITMAP_SIZE          0x00000020 // Size of scan-list bitmap.
static_assert(
  SCAN_LIST_SIZE == sizeof(D868UVCodeplug::scanlist_t),
  "D868UVCodeplug::scanlist_t size check failed.");

#define ADDR_GENERAL_CONFIG       0x02500000
#define GENERAL_CONFIG_SIZE       0x000000d0
static_assert(
  GENERAL_CONFIG_SIZE == sizeof(D868UVCodeplug::general_settings_base_t),
  "D868UVCodeplug::general_settings_base_t size check failed.");

#define ADDR_ZONE_CHANNELS        0x02500100
#define ZONE_CHANNELS_SIZE        0x00000400
static_assert(
  ZONE_CHANNELS_SIZE == sizeof(D868UVCodeplug::zone_channels_t),
  "D868UVCodeplug::zone_channels_t size check failed.");

#define ADDR_DTMF_NUMBERS         0x02500500
#define DTMF_NUMBERS_SIZE         0x00000100
static_assert(
  DTMF_NUMBERS_SIZE == sizeof(D868UVCodeplug::dtmf_numbers_t),
  "D868UVCodeplug::dtmf_numbers_t size check failed.");

#define ADDR_BOOT_SETTINGS        0x02500600
#define BOOT_SETTINGS_SIZE        0x00000030
static_assert(
  BOOT_SETTINGS_SIZE == sizeof(D868UVCodeplug::boot_settings_t),
  "D868UVCodeplug::boot_settings_t size check failed.");

#define ADDR_GPS_SETTINGS         0x02501000
#define GPS_SETTINGS_SIZE         0x00000030
static_assert(
  GPS_SETTINGS_SIZE == sizeof(D868UVCodeplug::gps_settings_t),
  "D868UVCodeplug::gps_settings_t size check failed.");

#define ADDR_GPS_MESSAGE          0x02501100
#define GPS_MESSAGE_SIZE          0x00000030

#define NUM_MESSAGES              100
#define NUM_MESSAGES_PER_BANK     8
#define MESSAGE_SIZE              0x00000100
#define MESSAGE_BANK_0            0x02140000
#define MESSAGE_BANK_SIZE         0x00000800
#define MESSAGE_BANK_OFFSET       0x00040000
#define MESSAGE_INDEX_LIST        0x01640000
#define MESSAGE_BYTEMAP           0x01640800
#define MESSAGE_BYTEMAP_SIZE      0x00000090
static_assert(
  MESSAGE_SIZE == sizeof(D868UVCodeplug::message_t),
  "D868UVCodeplug::grouplist_t size check failed.");

#define ADDR_HOTKEY               0x025C0000
#define HOTKEY_SIZE               0x00000860
#define STATUSMESSAGE_BITMAP      0x025C0B00
#define STATUSMESSAGE_BITMAP_SIZE 0x00000010

#define ADDR_OFFSET_FREQ          0x024C2000
#define OFFSET_FREQ_SIZE          0x000003F0

#define ADDR_ALARM_SETTING        0x024C1400
#define ALARM_SETTING_SIZE        0x00000020
static_assert(
  ALARM_SETTING_SIZE == sizeof(D868UVCodeplug::alarm_settings_t),
  "D868UVCodeplug::alarm_settings_t size check failed.");

#define ADDR_ALARM_SETTING_EXT    0x024c1440
#define ALARM_SETTING_EXT_SIZE    0x00000030
static_assert(
  ALARM_SETTING_EXT_SIZE == sizeof(D868UVCodeplug::digital_alarm_settings_ext_t),
  "D868UVCodeplug::digital_alarm_settings_ext_t size check failed.");

#define FMBC_BITMAP               0x02480210
#define FMBC_BITMAP_SIZE          0x00000020
#define ADDR_FMBC                 0x02480000
#define FMBC_SIZE                 0x00000200
#define ADDR_FMBC_VFO             0x02480200
#define FMBC_VFO_SIZE             0x00000010

#define FIVE_TONE_ID_BITMAP       0x024C0C80
#define FIVE_TONE_ID_BITMAP_SIZE  0x00000010
#define NUM_FIVE_TONE_IDS         100
#define ADDR_FIVE_TONE_ID_LIST    0x024C0000
#define FIVE_TONE_ID_SIZE         0x00000020
#define FIVE_TONE_ID_LIST_SIZE    0x00000c80
static_assert(
  FIVE_TONE_ID_SIZE == sizeof(D868UVCodeplug::five_tone_id_t),
  "D868UVCodeplug::five_tone_id_t size check failed.");
static_assert(
  FIVE_TONE_ID_LIST_SIZE == (NUM_FIVE_TONE_IDS*sizeof(D868UVCodeplug::five_tone_id_t)),
  "D868UVCodeplug::five_tone_function_t list size check failed.");
#define NUM_FIVE_TONE_FUNCTIONS   16
#define ADDR_FIVE_TONE_FUNCTIONS  0x024C0D00
#define FIVE_TONE_FUNCTION_SIZE   0x00000020
#define FIVE_TONE_FUNCTIONS_SIZE  0x00000200
static_assert(
  FIVE_TONE_FUNCTION_SIZE == sizeof(D868UVCodeplug::five_tone_function_t),
  "D868UVCodeplug::five_tone_function_t size check failed.");
static_assert(
  FIVE_TONE_FUNCTIONS_SIZE == (NUM_FIVE_TONE_FUNCTIONS*sizeof(D868UVCodeplug::five_tone_function_t)),
  "D868UVCodeplug::five_tone_function_t list size check failed.");
#define ADDR_FIVE_TONE_SETTINGS   0x024C1000
#define FIVE_TONE_SETTINGS_SIZE   0x00000080
static_assert(
  FIVE_TONE_SETTINGS_SIZE == sizeof(D868UVCodeplug::five_tone_settings_t),
  "D868UVCodeplug::five_tone_settings_t size check failed.");

#define ADDR_DTMF_SETTINGS        0x024C1080
#define DTMF_SETTINGS_SIZE        0x00000050
static_assert(
  DTMF_SETTINGS_SIZE == sizeof(D868UVCodeplug::dtmf_settings_t),
  "D868UVCodeplug::dtmf_settings_t size check failed.");

#define NUM_TWO_TONE_IDS          24
#define TWO_TONE_IDS_BITMAP       0x024C1280
#define TWO_TONE_IDS_BITMAP_SIZE  0x00000010
#define ADDR_TWO_TONE_IDS         0x024C1100
#define TWO_TONE_ID_SIZE          0x00000010
static_assert(
  TWO_TONE_ID_SIZE == sizeof(D868UVCodeplug::two_tone_id_t),
  "D868UVCodeplug::two_tone_settings_t size check failed.");

#define NUM_TWO_TONE_FUNCTIONS    16
#define TWO_TONE_FUNCTIONS_BITMAP 0x024c2600
#define TWO_TONE_FUNC_BITMAP_SIZE 0x00000010
#define ADDR_TWO_TONE_FUNCTIONS   0x024c2400
#define TWO_TONE_FUNCTION_SIZE    0x00000020
static_assert(
  TWO_TONE_FUNCTION_SIZE == sizeof(D868UVCodeplug::two_tone_function_t),
  "D868UVCodeplug::two_tone_settings_t size check failed.");

#define ADDR_TWO_TONE_SETTINGS    0x024C1290
#define TWO_TONE_SETTINGS_SIZE    0x00000010
static_assert(
  TWO_TONE_SETTINGS_SIZE == sizeof(D868UVCodeplug::two_tone_settings_t),
  "D868UVCodeplug::two_tone_settings_t size check failed.");

#define ADDR_DMR_ENCRYPTION_LIST  0x024C1700
#define DMR_ENCRYPTION_LIST_SIZE  0x00000040
#define ADDR_DMR_ENCRYPTION_KEYS  0x024C1800
#define DMR_ENCRYPTION_KEYS_SIZE  0x00000500

using namespace Signaling;

Code _ctcss_num2code[52] = {
  SIGNALING_NONE, // 62.5 not supported
  CTCSS_67_0Hz,  SIGNALING_NONE, // 69.3 not supported
  CTCSS_71_9Hz,  CTCSS_74_4Hz,  CTCSS_77_0Hz,  CTCSS_79_7Hz,  CTCSS_82_5Hz,
  CTCSS_85_4Hz,  CTCSS_88_5Hz,  CTCSS_91_5Hz,  CTCSS_94_8Hz,  CTCSS_97_4Hz,  CTCSS_100_0Hz,
  CTCSS_103_5Hz, CTCSS_107_2Hz, CTCSS_110_9Hz, CTCSS_114_8Hz, CTCSS_118_8Hz, CTCSS_123_0Hz,
  CTCSS_127_3Hz, CTCSS_131_8Hz, CTCSS_136_5Hz, CTCSS_141_3Hz, CTCSS_146_2Hz, CTCSS_151_4Hz,
  CTCSS_156_7Hz,
  SIGNALING_NONE, // 159.8 not supported
  CTCSS_162_2Hz,
  SIGNALING_NONE, // 165.5 not supported
  CTCSS_167_9Hz,
  SIGNALING_NONE, // 171.3 not supported
  CTCSS_173_8Hz,
  SIGNALING_NONE, // 177.3 not supported
  CTCSS_179_9Hz,
  SIGNALING_NONE, // 183.5 not supported
  CTCSS_186_2Hz,
  SIGNALING_NONE, // 189.9 not supported
  CTCSS_192_8Hz,
  SIGNALING_NONE, SIGNALING_NONE, // 196.6 & 199.5 not supported
  CTCSS_203_5Hz,
  SIGNALING_NONE, // 206.5 not supported
  CTCSS_210_7Hz, CTCSS_218_1Hz, CTCSS_225_7Hz,
  SIGNALING_NONE, // 229.1 not supported
  CTCSS_233_6Hz, CTCSS_241_8Hz, CTCSS_250_3Hz,
  SIGNALING_NONE, SIGNALING_NONE // 254.1 and custom CTCSS not supported.
};

uint8_t
D868UVCodeplug::ctcss_code2num(Signaling::Code code) {
  for (uint8_t i=0; i<52; i++) {
    if (code == _ctcss_num2code[i])
      return i;
  }
  return 0;
}

Signaling::Code
D868UVCodeplug::ctcss_num2code(uint8_t num) {
  if (num >= 52)
    return Signaling::SIGNALING_NONE;
  return _ctcss_num2code[num];
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::contact_t
 * ******************************************************************************************** */
D868UVCodeplug::contact_t::contact_t() {
  clear();
}

void
D868UVCodeplug::contact_t::clear() {
  memset(this, 0, sizeof(D868UVCodeplug::contact_t));
}

bool
D868UVCodeplug::contact_t::isValid() const {
  return (0 != name[0]) && (0xff != name[0]);
}

QString
D868UVCodeplug::contact_t::getName() const {
  return decode_ascii(name, 16, 0x00);
}

void
D868UVCodeplug::contact_t::setName(const QString &name) {
  encode_ascii(this->name, name, 16, 0);
}

DigitalContact::Type
D868UVCodeplug::contact_t::getType() const {
  switch ((CallType) type) {
  case CALL_PRIVATE: return DigitalContact::PrivateCall;
  case CALL_GROUP: return DigitalContact::GroupCall;
  case CALL_ALL: return DigitalContact::AllCall;
  }
}

void
D868UVCodeplug::contact_t::setType(DigitalContact::Type type) {
  switch (type) {
  case DigitalContact::PrivateCall: this->type = CALL_PRIVATE; break;
  case DigitalContact::GroupCall: this->type = CALL_GROUP; break;
  case DigitalContact::AllCall:
    this->type = CALL_ALL;
    this->id = qToBigEndian(16777215);
    break;
  }
}

uint32_t
D868UVCodeplug::contact_t::getId() const {
  return decode_dmr_id_bcd((const uint8_t *)&id);
}

void
D868UVCodeplug::contact_t::setId(uint32_t id) {
  encode_dmr_id_bcd((uint8_t *)&(this->id), id);
}

bool
D868UVCodeplug::contact_t::getAlert() const {
  return (ALERT_NONE != call_alert);
}

void
D868UVCodeplug::contact_t::setAlert(bool enable) {
  call_alert = enable ? ALERT_RING : ALERT_NONE;
}

DigitalContact *
D868UVCodeplug::contact_t::toContactObj() const {
  return new DigitalContact(getType(), getName(), getId(), getAlert());
}

void
D868UVCodeplug::contact_t::fromContactObj(const DigitalContact *contact) {
  clear();
  setType(contact->type());
  setName(contact->name());
  setId(contact->number());
  setAlert(contact->ring());
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::analog_contact_t
 * ******************************************************************************************** */
void
D868UVCodeplug::analog_contact_t::clear() {
  memset(number, 0, sizeof(number));
  digits = 0;
  memset(name, 0, sizeof(name));
  pad47 = 0;
}

QString
D868UVCodeplug::analog_contact_t::getNumber() const {
  return decode_dtmf_bcd_be(number, digits);
}

bool
D868UVCodeplug::analog_contact_t::setNumber(const QString &num) {
  if (! validDTMFNumber(num))
    return false;
  digits = num.length();
  return encode_dtmf_bcd_be(num, number, sizeof(number), 0);
}

QString
D868UVCodeplug::analog_contact_t::getName() const {
  return decode_ascii(name, sizeof(name), 0);
}

void
D868UVCodeplug::analog_contact_t::setName(const QString &name) {
  encode_ascii(this->name, name, sizeof(this->name), 0);
}

void
D868UVCodeplug::analog_contact_t::fromContact(const DTMFContact *contact) {
  setNumber(contact->number());
  setName(contact->name());
}

DTMFContact *
D868UVCodeplug::analog_contact_t::toContact() const {
  return new DTMFContact(getName(), getNumber());
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::grouplist_t
 * ******************************************************************************************** */
D868UVCodeplug::grouplist_t::grouplist_t() {
  clear();
}

void
D868UVCodeplug::grouplist_t::clear() {
  memset(member, 0xff, sizeof(member));
  memset(name, 0x00, sizeof(name));
  memset(unused, 0x00, sizeof(unused));
}

bool
D868UVCodeplug::grouplist_t::isValid() const {
  return (0 != name[0]) && (0xff != name[0]);
}

QString
D868UVCodeplug::grouplist_t::getName() const {
  return decode_ascii(name, 16, 0x00);
}

void
D868UVCodeplug::grouplist_t::setName(const QString &name) {
  encode_ascii(this->name, name, 16, 0x00);
}

RXGroupList *
D868UVCodeplug::grouplist_t::toGroupListObj() const {
  return new RXGroupList(getName());
}

bool
D868UVCodeplug::grouplist_t::linkGroupList(RXGroupList *lst, const CodeplugContext &ctx) {
  for (uint8_t i=0; i<64; i++) {
    uint32_t idx = qFromLittleEndian(member[i]);
    // Disabled contact -> continue
    if (0xffffffff == idx)
      continue;
    // Missing contact ignore.
    if (! ctx.hasDigitalContact(idx)) {
      logWarn() << "Cannot link contact " << member[i] << " to group list '"
                << this->getName() << "': Invalid contact index. Ignored.";
      continue;
    }

    lst->addContact(ctx.getDigitalContact(idx));
  }
  return true;
}

void
D868UVCodeplug::grouplist_t::fromGroupListObj(const RXGroupList *lst, const Config *conf) {
  clear();
  // set name of group list
  setName(lst->name());
  // set members
  for (uint8_t i=0; i<64; i++) {
    if (i < lst->count())
      member[i] = qToLittleEndian(conf->contacts()->indexOfDigital(lst->contact(i)));
    else
      member[i] = 0xffffffff;
  }
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::scanlist_t
 * ******************************************************************************************** */
D868UVCodeplug::scanlist_t::scanlist_t() {
  clear();
}

void
D868UVCodeplug::scanlist_t::clear() {
  _unused0000 = 0;
  prio_ch_select = PRIO_CHAN_OFF;
  priority_ch1 = 0xffff;
  priority_ch2 = 0xffff;
  look_back_a = qToLittleEndian(0x000f);
  look_back_b = qToLittleEndian(0x0019);
  dropout_delay = qToLittleEndian(0x001d);
  dwell = qToLittleEndian(0x001d);
  revert_channel = REVCH_SELECTED;
  memset(name, 0, sizeof(name));
  _pad001e = 0;
  memset(member, 0xff, sizeof(member));
  memset(_unused0084, 0, sizeof(_unused0084));
}

QString
D868UVCodeplug::scanlist_t::getName() const {
  return decode_ascii(name, sizeof(name), 0);
}

void
D868UVCodeplug::scanlist_t::setName(const QString &name) {
  encode_ascii(this->name, name, 16, 0);
}

ScanList *
D868UVCodeplug::scanlist_t::toScanListObj() {
  return new ScanList(getName());
}

void
D868UVCodeplug::scanlist_t::linkScanListObj(ScanList *lst, CodeplugContext &ctx) {
  if (prio_ch_select & PRIO_CHAN_SEL1) {
    uint16_t idx = qFromLittleEndian(priority_ch1);
    if (! ctx.hasChannel(idx)) {
      logError() << "Cannot link scanlist '" << getName()
                 << "', priority channel 1 index " << idx << " unknown.";
      // Ignore error, continue decoding
    } else {
      lst->setPrimaryChannel(ctx.getChannel(idx));
    }
  }

  if (prio_ch_select & PRIO_CHAN_SEL2) {
    uint16_t idx = qFromLittleEndian(priority_ch2);
    if (! ctx.hasChannel(idx)) {
      logError() << "Cannot link scanlist '" << getName()
                 << "', priority channel 2 index " << idx << " unknown.";
      // Ignore error, continue decoding
    } else {
      lst->setSecondaryChannel(ctx.getChannel(idx));
    }
  }

  for (uint16_t i=0; i<50; i++) {
    if (0xffff == member[i])
      continue;
    uint16_t idx = qFromLittleEndian(member[i]);
    if (! ctx.hasChannel(idx)) {
      logError() << "Cannot link scanlist '" << getName() << "', channel index " << idx << " unknown.";
      continue;
    }
    lst->addChannel(ctx.getChannel(idx));
  }
}

bool
D868UVCodeplug::scanlist_t::fromScanListObj(ScanList *lst, Config *config) {
  clear();

  setName(lst->name());
  prio_ch_select = PRIO_CHAN_OFF;

  if (lst->primaryChannel()) {
    prio_ch_select |= PRIO_CHAN_SEL1;
    if (SelectedChannel::get() == lst->primaryChannel())
      priority_ch1 = 0x0000;
    else
      priority_ch1 = qToLittleEndian(
            config->channelList()->indexOf(lst->primaryChannel())+1);
  }

  if (lst->secondaryChannel()) {
    prio_ch_select |= PRIO_CHAN_SEL2;
    if (SelectedChannel::get() == lst->secondaryChannel())
      priority_ch2 = 0x0000;
    else
      priority_ch2 = qToLittleEndian(
            config->channelList()->indexOf(lst->secondaryChannel())+1);
  }

  for (int i=0; i<std::min(50, lst->count()); i++) {
    if (SelectedChannel::get() == lst->channel(i))
      continue;
    member[i] = qToLittleEndian(
          config->channelList()->indexOf(lst->channel(i)));
  }

  return false;
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::radioid_t
 * ******************************************************************************************** */
D868UVCodeplug::radioid_t::radioid_t() {
  clear();
}

void
D868UVCodeplug::radioid_t::clear() {
  memset(this, 0, sizeof(D868UVCodeplug::radioid_t));
}

bool
D868UVCodeplug::radioid_t::isValid() const {
  return ((0x00 != name[0]) && (0xff != name[0]));
}

QString
D868UVCodeplug::radioid_t::getName() const {
  return decode_ascii(name, 16, 0);
}

void
D868UVCodeplug::radioid_t::setName(const QString name) {
  encode_ascii(this->name, name, 16, 0);
}

uint32_t
D868UVCodeplug::radioid_t::getId() const {
  uint32_t id_bcd = qFromLittleEndian(this->id);
  return decode_dmr_id_bcd((const uint8_t *) &id_bcd);
}

void
D868UVCodeplug::radioid_t::setId(uint32_t num) {
  uint32_t id_bcd;
  encode_dmr_id_bcd((uint8_t *)&id_bcd, num);
  this->id = qToLittleEndian(id_bcd);
}



/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::general_settings_base_t
 * ******************************************************************************************** */
void
D868UVCodeplug::general_settings_base_t::fromConfig(Config *config, const Flags &flags) {
  // Set microphone gain
  mic_gain = std::min(uint(4), config->micLevel()/10);

  // If auto-enable GPS is enabled
  if (flags.autoEnableGPS) {
    // Check if GPS is required -> enable
    if (config->requiresGPS()) {
      enable_gps = 0x01;
      // Set time zone based on system time zone.
      int offset = QTimeZone::systemTimeZone().offsetFromUtc(QDateTime::currentDateTime());
      gps_timezone = 12 + offset/3600;
      enable_get_gps_pos = 0x00;
      gps_update_period = 0x05;
      // Set measurement system based on system locale (0x00==Metric)
      gps_units = (QLocale::MetricSystem == QLocale::system().measurementSystem()) ? GPS_METRIC : GPS_IMPERIAL;
    } else {
      enable_gps = 0x00;
    }
  }
}

void
D868UVCodeplug::general_settings_base_t::updateConfig(Config *config) {
  // get microphone gain
  config->setMicLevel(2*mic_gain+1);
  // D868UV does not support speech synthesis?
  config->setSpeech(false);
}



/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::boot_settings_t
 * ******************************************************************************************** */
void
D868UVCodeplug::boot_settings_t::clear() {
  memset(intro_line1, 0, sizeof(intro_line2));
  memset(intro_line2, 0, sizeof(intro_line2));
}

QString
D868UVCodeplug::boot_settings_t::getIntroLine1() const {
  return decode_ascii(intro_line1, 14, 0);
}
void
D868UVCodeplug::boot_settings_t::setIntroLine1(const QString line) {
  encode_ascii(intro_line1, line, 14, 0);
}

QString
D868UVCodeplug::boot_settings_t::getIntroLine2() const {
  return decode_ascii(intro_line2, 14, 0);
}
void
D868UVCodeplug::boot_settings_t::setIntroLine2(const QString line) {
  encode_ascii(intro_line2, line, 14, 0);
}

void
D868UVCodeplug::boot_settings_t::fromConfig(const Config *config, const Flags &flags) {
  Q_UNUSED(flags)
  setIntroLine1(config->introLine1());
  setIntroLine2(config->introLine2());
}

void
D868UVCodeplug::boot_settings_t::updateConfig(Config *config) {
  config->setIntroLine1(getIntroLine1());
  config->setIntroLine2(getIntroLine2());
}



/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::gps_settings_t
 * ******************************************************************************************** */
void
D868UVCodeplug::gps_settings_t::clear() {
  manual_tx_intervall = 0;
  auto_tx_intervall = 0;
  enable_fixed_location = 0;
  transmit_power = POWER_LOW;
  for (uint8_t i=0; i<8; i++)
    channel_idxs[i] = qToLittleEndian(0x0fa1);
  target_id = 1;
  call_type = PRIVATE_CALL;
  timeslot = TIMESLOT_SAME;
}

uint8_t
D868UVCodeplug::gps_settings_t::getManualTXIntervall() const {
  return manual_tx_intervall;
}
void
D868UVCodeplug::gps_settings_t::setManualTXIntervall(uint8_t period) {
  manual_tx_intervall = period;
}

uint16_t
D868UVCodeplug::gps_settings_t::getAutomaticTXIntervall() const {
  if (0 == auto_tx_intervall)
    return 0;
  return 45 + 15*auto_tx_intervall;
}
void
D868UVCodeplug::gps_settings_t::setAutomaticTXIntervall(uint16_t period) {
  if (0 == period)
    auto_tx_intervall = 0;
  else if (60 > period)
    auto_tx_intervall = 1;
  else
    auto_tx_intervall = (period-45)/15;
}

Channel::Power
D868UVCodeplug::gps_settings_t::getTransmitPower() const {
  switch (transmit_power) {
  case POWER_LOW: return Channel::Power::Low;
  case POWER_MID: return Channel::Power::Mid;
  case POWER_HIGH: return Channel::Power::High;
  case POWER_TURBO: return Channel::Power::Max;
  }
}
void
D868UVCodeplug::gps_settings_t::setTransmitPower(Channel::Power power) {
  switch(power) {
  case Channel::Power::Min:
  case Channel::Power::Low:
    transmit_power = POWER_LOW;
    break;
  case Channel::Power::Mid:
    transmit_power = POWER_MID;
    break;
  case Channel::Power::High:
    transmit_power = POWER_HIGH;
    break;
  case Channel::Power::Max:
    transmit_power = POWER_TURBO;
    break;
  }
}

bool
D868UVCodeplug::gps_settings_t::isFixedLocation() const {
  return enable_fixed_location;
}

double
D868UVCodeplug::gps_settings_t::getFixedLat() const {
  return ((1 == north_south) ? -1 : 1) * (lat_deg + double(lat_min)/60 + double(lat_sec)/3600);
}

double
D868UVCodeplug::gps_settings_t::getFixedLon() const {
  return ((1 == east_west) ? -1 : 1) * (lon_deg + double(lon_min)/60 + double(lon_sec)/3600);
}

void
D868UVCodeplug::gps_settings_t::setFixedLocation(double lat, double lon) {
  enable_fixed_location = 1;
  north_south = (lat < 0) ? 1 : 0; lat = std::abs(lat);
  lat_deg = uint(lat); lat -= uint(lat); lat *= 60;
  lat_min = uint(lat); lat -= uint(lat); lat *= 60;
  lat_sec = uint(lat);
  east_west = (lon < 0) ? 1 : 0; lon = std::abs(lon);
  lon_deg = uint(lon); lon -= uint(lon); lon *= 60;
  lon_min = uint(lon); lon -= uint(lon); lon *= 60;
  lon_sec = uint(lon);
}

bool
D868UVCodeplug::gps_settings_t::isChannelSelected(uint8_t i) const {
  return 0x0fa2 == qFromLittleEndian(channel_idxs[i]);
}

bool
D868UVCodeplug::gps_settings_t::isChannelVFOA(uint8_t i) const {
  return 0x0fa0 == qFromLittleEndian(channel_idxs[i]);
}

bool
D868UVCodeplug::gps_settings_t::isChannelVFOB(uint8_t i) const {
  return 0x0fa1 == qFromLittleEndian(channel_idxs[i]);
}

uint16_t
D868UVCodeplug::gps_settings_t::getChannelIndex(uint8_t i) const {
  return qFromLittleEndian(channel_idxs[i]);
}

void
D868UVCodeplug::gps_settings_t::setChannelIndex(uint8_t i, uint16_t idx) {
  channel_idxs[i] = qToLittleEndian(idx);
}

void
D868UVCodeplug::gps_settings_t::setChannelSelected(uint8_t i) {
  channel_idxs[i] = qToLittleEndian(0x0fa2);
}

void
D868UVCodeplug::gps_settings_t::setChannelVFOA(uint8_t i) {
  channel_idxs[i] = qToLittleEndian(0x0fa0);
}

void
D868UVCodeplug::gps_settings_t::setChannelVFOB(uint8_t i) {
  channel_idxs[i] = qToLittleEndian(0x0fa1);
}

uint32_t
D868UVCodeplug::gps_settings_t::getTargetID() const {
  return decode_dmr_id_bcd((uint8_t *)&target_id);
}
void
D868UVCodeplug::gps_settings_t::setTargetID(uint32_t id) {
  encode_dmr_id_bcd((uint8_t *)&target_id, id);
}

DigitalContact::Type
D868UVCodeplug::gps_settings_t::getTargetType() const {
  switch (call_type) {
  case PRIVATE_CALL: return DigitalContact::PrivateCall;
  case GROUP_CALL: return DigitalContact::GroupCall;
  case ALL_CALL: return DigitalContact::AllCall;
  }
}
void
D868UVCodeplug::gps_settings_t::setTargetType(DigitalContact::Type type) {
  switch(type) {
  case DigitalContact::PrivateCall: call_type = PRIVATE_CALL; break;
  case DigitalContact::GroupCall: call_type = GROUP_CALL; break;
  case DigitalContact::AllCall: call_type = ALL_CALL; break;
  }
}

void
D868UVCodeplug::gps_settings_t::fromConfig(Config *config, const Flags &flags) {
  if (1 < config->posSystems()->gpsCount()) {
    logDebug() << "D868UV only supports a single independent GPS positioning system.";
  } else if (0 == config->posSystems()->gpsCount()) {
    return;
  }

  GPSSystem *sys = config->posSystems()->gpsSystem(0);
  setTargetID(sys->contactObj()->number());
  setTargetType(sys->contactObj()->type());
  setManualTXIntervall(sys->period());
  setAutomaticTXIntervall(sys->period());
  if (SelectedChannel::get() == sys->revertChannel()->as<Channel>()) {
    setChannelSelected(0);
    timeslot = TIMESLOT_SAME;
  } else {
    setChannelIndex(0, config->channelList()->indexOf(sys->revertChannel()));
    timeslot = TIMESLOT_SAME;
  }
}

bool
D868UVCodeplug::gps_settings_t::createGPSSystem(uint8_t i, Config *config, CodeplugContext &ctx) {
  ctx.addGPSSystem(new GPSSystem(QString("GPS sys %1").arg(i+1), nullptr, nullptr, getAutomaticTXIntervall()), i);
  return true;
}

bool
D868UVCodeplug::gps_settings_t::linkGPSSystem(uint8_t i, Config *config, CodeplugContext &ctx) {
  DigitalContact *cont = nullptr;
  // Find matching contact, if not found -> create one.
  if (nullptr == (cont = config->contacts()->findDigitalContact(getTargetID()))) {
    cont = new DigitalContact(getTargetType(), QString("GPS target"), getTargetID());
    config->contacts()->add(cont);
  }
  ctx.getGPSSystem(i)->setContact(cont);

  // Check if there is a revert channel set
  if ((! isChannelSelected(i)) && (ctx.hasChannel(getChannelIndex(i))) && (ctx.getChannel(getChannelIndex(i)))->is<DigitalChannel>()) {
    DigitalChannel *ch = ctx.getChannel(getChannelIndex(i))->as<DigitalChannel>();
    ctx.getGPSSystem(i)->setRevertChannel(ch);
  }
  return true;
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::contact_map_t
 * ******************************************************************************************** */
D868UVCodeplug::contact_map_t::contact_map_t() {
  clear();
}

void
D868UVCodeplug::contact_map_t::clear() {
  memset(this, 0xff, sizeof(contact_map_t));
}

bool
D868UVCodeplug::contact_map_t::isValid() const {
  return (0xffffffff != id_group) && (0xffffffff != contact_index);
}

bool
D868UVCodeplug::contact_map_t::isGroup() const {
  uint32_t tmp = qFromLittleEndian(id_group);
  return tmp & 0x01;
}
uint32_t
D868UVCodeplug::contact_map_t::ID() const {
  uint32_t tmp = qFromLittleEndian(id_group);
  tmp = tmp>>1;
  return decode_dmr_id_bcd_le((uint8_t *)&tmp);
}
void
D868UVCodeplug::contact_map_t::setID(uint32_t id, bool group) {
  uint32_t tmp; encode_dmr_id_bcd_le((uint8_t *)&tmp, id);
  tmp = ( (tmp << 1) | (group ? 1 : 0) );
  id_group = qToLittleEndian(tmp);
}


uint32_t
D868UVCodeplug::contact_map_t::index() const {
  return qFromLittleEndian(contact_index);
}
void
D868UVCodeplug::contact_map_t::setIndex(uint32_t index) {
  contact_index = qToLittleEndian(index);
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug::GeneralSettingsElement
 * ******************************************************************************************** */
D868UVCodeplug::GeneralSettingsElement::GeneralSettingsElement(uint8_t *ptr, uint size)
  : AnytoneCodeplug::GeneralSettingsElement(ptr, size)
{
  // pass....
}

D868UVCodeplug::GeneralSettingsElement::GeneralSettingsElement(uint8_t *ptr)
  : D868UVCodeplug::GeneralSettingsElement(ptr, 0x00d0)
{
  // pass...
}

void
D868UVCodeplug::GeneralSettingsElement::clear() {
  AnytoneCodeplug::GeneralSettingsElement::clear();
}

D868UVCodeplug::GeneralSettingsElement::Color
D868UVCodeplug::GeneralSettingsElement::callDisplayColor() const {
  return (Color)getUInt8(0x00b0);
}
void
D868UVCodeplug::GeneralSettingsElement::setCallDisplayColor(Color color) {
  setUInt8(0x00b0, (uint)color);
}

uint
D868UVCodeplug::GeneralSettingsElement::gpsUpdatePeriod() const {
  return getUInt8(0x00b1);
}
void
D868UVCodeplug::GeneralSettingsElement::setGPSUpdatePeriod(uint sec) {
  setUInt8(0x00b1, sec);
}

bool
D868UVCodeplug::GeneralSettingsElement::showZoneAndContact() const {
  return getUInt8(0x00b2);
}
void
D868UVCodeplug::GeneralSettingsElement::enableShowZoneAndContact(bool enable) {
  setUInt8(0x00b2, (enable ? 0x01 : 0x00));
}

bool
D868UVCodeplug::GeneralSettingsElement::keyToneLevelAdjustable() const {
  return 0 == keyToneLevel();
}
uint
D868UVCodeplug::GeneralSettingsElement::keyToneLevel() const {
  return ((uint)getUInt8(0x00b3))*10/15;
}
void
D868UVCodeplug::GeneralSettingsElement::setKeyToneLevel(uint level) {
  setUInt8(0x00b3, level*10/15);
}
void
D868UVCodeplug::GeneralSettingsElement::setKeyToneLevelAdjustable() {
  setUInt8(0x00b3, 0);
}

bool
D868UVCodeplug::GeneralSettingsElement::gpsUnitsImperial() const {
  return getUInt8(0x00b4);
}
void
D868UVCodeplug::GeneralSettingsElement::enableGPSUnitsImperial(bool enable) {
  setUInt8(0x00b4, (enable ? 0x01 : 0x00));
}

bool
D868UVCodeplug::GeneralSettingsElement::knobLock() const {
  return getBit(0x00b5, 0);
}
void
D868UVCodeplug::GeneralSettingsElement::enableKnobLock(bool enable) {
  setBit(0x00b5, 0, enable);
}
bool
D868UVCodeplug::GeneralSettingsElement::keypadLock() const {
  return getBit(0x00b5, 1);
}
void
D868UVCodeplug::GeneralSettingsElement::enableKeypadLock(bool enable) {
  setBit(0x00b5, 1, enable);
}
bool
D868UVCodeplug::GeneralSettingsElement::sidekeysLock() const {
  return getBit(0x00b5, 3);
}
void
D868UVCodeplug::GeneralSettingsElement::enableSidekeysLock(bool enable) {
  setBit(0x00b5, 3, enable);
}
bool
D868UVCodeplug::GeneralSettingsElement::keyLockForced() const {
  return getBit(0x00b5, 4);
}
void
D868UVCodeplug::GeneralSettingsElement::enableKeyLockForced(bool enable) {
  setBit(0x00b5, 4, enable);
}

bool
D868UVCodeplug::GeneralSettingsElement::showLastHeard() const {
  return getUInt8(0x00b6);
}
void
D868UVCodeplug::GeneralSettingsElement::enableShowLastHeard(bool enable) {
  setUInt8(0x00b6, (enable ? 0x01 : 0x00));
}

uint
D868UVCodeplug::GeneralSettingsElement::autoRepeaterMinFrequencyVHF() const {
  return getBCD8_le(0x00b8)*10;
}
void
D868UVCodeplug::GeneralSettingsElement::setAutoRepeaterMinFrequencyVHF(uint Hz) {
  setBCD8_le(0x00b8, Hz/10);
}
uint
D868UVCodeplug::GeneralSettingsElement::autoRepeaterMaxFrequencyVHF() const {
  return getBCD8_le(0x00bc)*10;
}
void
D868UVCodeplug::GeneralSettingsElement::setAutoRepeaterMaxFrequencyVHF(uint Hz) {
  setBCD8_le(0x00bc, Hz/10);
}

uint
D868UVCodeplug::GeneralSettingsElement::autoRepeaterMinFrequencyUHF() const {
  return getBCD8_le(0x00c0)*10;
}
void
D868UVCodeplug::GeneralSettingsElement::setAutoRepeaterMinFrequencyUHF(uint Hz) {
  setBCD8_le(0x00c0, Hz/10);
}
uint
D868UVCodeplug::GeneralSettingsElement::autoRepeaterMaxFrequencyUHF() const {
  return getBCD8_le(0x00c4)*10;
}
void
D868UVCodeplug::GeneralSettingsElement::setAutoRepeaterMaxFrequencyUHF(uint Hz) {
  setBCD8_le(0x00c4, Hz/10);
}

D868UVCodeplug::GeneralSettingsElement::AutoRepDir
D868UVCodeplug::GeneralSettingsElement::autoRepeaterDirectionB() const {
  return (AutoRepDir)getUInt8(0x00c8);
}
void
D868UVCodeplug::GeneralSettingsElement::setAutoRepeaterDirectionB(AutoRepDir dir) {
  setUInt8(0x00c8, (uint)dir);
}

bool
D868UVCodeplug::GeneralSettingsElement::defaultChannel() const {
  return getUInt8(0x00ca);
}
void
D868UVCodeplug::GeneralSettingsElement::enableDefaultChannel(bool enable) {
  setUInt8(0x00ca, (enable ? 0x01 : 0x00));
}

uint
D868UVCodeplug::GeneralSettingsElement::defaultZoneIndexA() const {
  return getUInt8(0x00cb);
}
void
D868UVCodeplug::GeneralSettingsElement::setDefaultZoneIndexA(uint idx) {
  setUInt8(0x00cb, idx);
}

uint
D868UVCodeplug::GeneralSettingsElement::defaultZoneIndexB() const {
  return getUInt8(0x00cc);
}
void
D868UVCodeplug::GeneralSettingsElement::setDefaultZoneIndexB(uint idx) {
  setUInt8(0x00cc, idx);
}

bool
D868UVCodeplug::GeneralSettingsElement::defaultChannelAIsVFO() const {
  return 0xff == defaultChannelAIndex();
}
uint
D868UVCodeplug::GeneralSettingsElement::defaultChannelAIndex() const {
  return getUInt8(0x00cd);
}
void
D868UVCodeplug::GeneralSettingsElement::setDefaultChannelAIndex(uint idx) {
  setUInt8(0x00cd, idx);
}
void
D868UVCodeplug::GeneralSettingsElement::setDefaultChannelAToVFO() {
  setDefaultChannelAIndex(0xff);
}

bool
D868UVCodeplug::GeneralSettingsElement::defaultChannelBIsVFO() const {
  return 0xff == defaultChannelBIndex();
}
uint
D868UVCodeplug::GeneralSettingsElement::defaultChannelBIndex() const {
  return getUInt8(0x00ce);
}
void
D868UVCodeplug::GeneralSettingsElement::setDefaultChannelBIndex(uint idx) {
  setUInt8(0x00ce, idx);
}
void
D868UVCodeplug::GeneralSettingsElement::setDefaultChannelBToVFO() {
  setDefaultChannelBIndex(0xff);
}

bool
D868UVCodeplug::GeneralSettingsElement::keepLastCaller() const {
  return getUInt8(0x00cf);
}
void
D868UVCodeplug::GeneralSettingsElement::enableKeepLastCaller(bool enable) {
  setUInt8(0x00cf, (enable ? 0x01 : 0x00));
}

bool
D868UVCodeplug::GeneralSettingsElement::fromConfig(const Flags &flags, Context &ctx) {
  if (! AnytoneCodeplug::GeneralSettingsElement::fromConfig(flags, ctx))
    return false;

  setGPSUpdatePeriod(0x05);
  // Set measurement system based on system locale (0x00==Metric)
  enableGPSUnitsImperial(QLocale::ImperialSystem == QLocale::system().measurementSystem());

  return true;
}

bool
D868UVCodeplug::GeneralSettingsElement::updateConfig(Context &ctx) {
  if (! AnytoneCodeplug::GeneralSettingsElement::updateConfig(ctx))
    return false;
  return true;
}


/* ******************************************************************************************** *
 * Implementation of D868UVCodeplug
 * ******************************************************************************************** */
D868UVCodeplug::D868UVCodeplug(QObject *parent)
  : AnytoneCodeplug(parent)
{
  addImage("Anytone AT-D868UV Codeplug");

  // Channel bitmap
  image(0).addElement(CHANNEL_BITMAP, CHANNEL_BITMAP_SIZE);
  // Zone bitmap
  image(0).addElement(ZONE_BITMAPS, ZONE_BITMAPS_SIZE);
  // Contacts bitmap
  image(0).addElement(CONTACTS_BITMAP, CONTACTS_BITMAP_SIZE);
  // Analog contacts bytemap
  image(0).addElement(ANALOGCONTACT_BYTEMAP, ANALOGCONTACT_BYTEMAP_SIZE);
  // RX group list bitmaps
  image(0).addElement(RXGRP_BITMAP, RXGRP_BITMAP_SIZE);
  // Scan list bitmaps
  image(0).addElement(SCAN_BITMAP, SCAN_BITMAP_SIZE);
  // Radio IDs bitmaps
  image(0).addElement(RADIOID_BITMAP, RADIOID_BITMAP_SIZE);
  // Messag bitmaps
  image(0).addElement(MESSAGE_BYTEMAP, MESSAGE_BYTEMAP_SIZE);
  // Status messages
  image(0).addElement(STATUSMESSAGE_BITMAP, STATUSMESSAGE_BITMAP_SIZE);
  // FM Broadcast bitmaps
  image(0).addElement(FMBC_BITMAP, FMBC_BITMAP_SIZE);
  // 5-Tone function bitmaps
  image(0).addElement(FIVE_TONE_ID_BITMAP, FIVE_TONE_ID_BITMAP_SIZE);
  // 2-Tone function bitmaps
  image(0).addElement(TWO_TONE_IDS_BITMAP, TWO_TONE_IDS_BITMAP_SIZE);
  image(0).addElement(TWO_TONE_FUNCTIONS_BITMAP, TWO_TONE_FUNC_BITMAP_SIZE);
}

void
D868UVCodeplug::clear() {
  // NOOP
}

void
D868UVCodeplug::allocateUpdated() {
  this->allocateVFOSettings();

  // General config
  this->allocateGeneralSettings();
  this->allocateZoneChannelList();
  this->allocateDTMFNumbers();
  this->allocateBootSettings();

  this->allocateGPSSystems();

  this->allocateSMSMessages();
  this->allocateHotKeySettings();
  this->allocateRepeaterOffsetSettings();
  this->allocateAlarmSettings();
  this->allocateFMBroadcastSettings();

  this->allocate5ToneIDs();
  this->allocate5ToneFunctions();
  this->allocate5ToneSettings();

  this->allocate2ToneIDs();
  this->allocate2ToneFunctions();
  this->allocate2ToneSettings();

  this->allocateDTMFSettings();

  image(0).addElement(ADDR_DMR_ENCRYPTION_LIST, DMR_ENCRYPTION_LIST_SIZE);
  image(0).addElement(ADDR_DMR_ENCRYPTION_KEYS, DMR_ENCRYPTION_KEYS_SIZE);
}

void
D868UVCodeplug::allocateForEncoding() {
  this->allocateChannels();
  this->allocateZones();
  this->allocateContacts();
  this->allocateAnalogContacts();
  this->allocateRXGroupLists();
  this->allocateScanLists();
  this->allocateRadioIDs();
}

void
D868UVCodeplug::allocateForDecoding() {
  this->allocateRadioIDs();
  this->allocateChannels();
  this->allocateZones();
  this->allocateContacts();
  this->allocateAnalogContacts();
  this->allocateRXGroupLists();
  this->allocateScanLists();

  // General config
  this->allocateGeneralSettings();
  this->allocateZoneChannelList();
  this->allocateBootSettings();

  // GPS settings
  this->allocateGPSSystems();
}


void
D868UVCodeplug::setBitmaps(Config *config)
{
  // Mark first radio ID as valid
  uint8_t *radioid_bitmap = data(RADIOID_BITMAP);
  memset(radioid_bitmap, 0, RADIOID_BITMAP_SIZE);
  for (int i=0; i<std::min(NUM_RADIOIDS, config->radioIDs()->count()); i++)
    radioid_bitmap[i/8] |= (1 << (i%8));

  // Mark valid channels (set bit)
  uint8_t *channel_bitmap = data(CHANNEL_BITMAP);
  memset(channel_bitmap, 0, CHANNEL_BITMAP_SIZE);
  for (int i=0; i<std::min(NUM_CHANNELS, config->channelList()->count()); i++) {
    channel_bitmap[i/8] |= (1 << (i%8));
  }

  // Mark valid contacts (clear bit)
  uint8_t *contact_bitmap = data(CONTACTS_BITMAP);
  memset(contact_bitmap, 0x00, CONTACTS_BITMAP_SIZE);
  memset(contact_bitmap, 0xff, NUM_CONTACTS/8+1);
  for (int i=0; i<std::min(NUM_CONTACTS, config->contacts()->digitalCount()); i++) {
    contact_bitmap[i/8] &= ~(1 << (i%8));
  }

  // Mark valid analog contacts (clear bytes)
  uint8_t *analog_contact_bitmap = data(ANALOGCONTACT_BYTEMAP);
  memset(analog_contact_bitmap, 0xff, ANALOGCONTACT_BYTEMAP_SIZE);
  for (int i=0; i<std::min(NUM_ANALOGCONTACTS, config->contacts()->dtmfCount()); i++) {
    analog_contact_bitmap[i] = 0x00;
  }

  // Mark valid zones (set bits)
  uint8_t *zone_bitmap = data(ZONE_BITMAPS);
  memset(zone_bitmap, 0x00, ZONE_BITMAPS_SIZE);
  for (int i=0,z=0; i<std::min(NUM_ZONES, config->zones()->count()); i++) {
    zone_bitmap[z/8] |= (1 << (z%8)); z++;
    if (config->zones()->zone(i)->B()->count()) {
      zone_bitmap[z/8] |= (1 << (z%8)); z++;
    }
  }

  // Mark group lists
  uint8_t *group_bitmap = data(RXGRP_BITMAP);
  memset(group_bitmap, 0x00, RXGRP_BITMAP_SIZE);
  for (int i=0; i<std::min(NUM_RXGRP, config->rxGroupLists()->count()); i++)
    group_bitmap[i/8] |= (1 << (i%8));

  // Mark scan lists
  uint8_t *scan_bitmap = data(SCAN_BITMAP);
  memset(scan_bitmap, 0x00, SCAN_BITMAP_SIZE);
  for (int i=0; i<std::min(NUM_SCAN_LISTS, config->scanlists()->count()); i++) {
    scan_bitmap[i/8] |= (1<<(i%8));
  }
}

bool
D868UVCodeplug::encode(Config *config, const Flags &flags) {
  Context ctx(config);
  if (! index(config, ctx))
    return false;

  return encodeElements(flags, ctx);
}

bool D868UVCodeplug::decode(Config *config) {
  // Maps code-plug indices to objects
  Context ctx(config);
  return decodeElements(ctx);
}

bool
D868UVCodeplug::encodeElements(const Flags &flags, Context &ctx)
{
  if (! this->encodeRadioID(flags, ctx))
    return false;

  if (! this->encodeGeneralSettings(flags, ctx))
    return false;

  if (! this->encodeBootSettings(flags, ctx))
    return false;

  if (! this->encodeChannels(flags, ctx))
    return false;

  if (! this->encodeContacts(flags, ctx))
    return false;

  if (! this->encodeAnalogContacts(flags, ctx))
    return false;

  if (! this->encodeRXGroupLists(flags, ctx))
    return false;

  if (! this->encodeZones(flags, ctx))
    return false;

  if (! this->encodeScanLists(flags, ctx))
    return false;

  if (! this->encodeGPSSystems(flags, ctx))
    return false;

  return true;
}

bool
D868UVCodeplug::decodeElements(Context &ctx)
{
  if (! this->setRadioID(ctx))
    return false;

  if (! this->decodeGeneralSettings(ctx))
    return false;

  if (! this->decodeBootSettings(ctx))
    return false;

  if (! this->createChannels(ctx))
    return false;

  if (! this->createContacts(ctx))
    return false;

  if (! this->createAnalogContacts(ctx))
    return false;

  if (! this->createRXGroupLists(ctx))
    return false;

  if (! this->linkRXGroupLists(ctx))
    return false;

  if (! this->createZones(ctx))
    return false;

  if (! this->linkZones(ctx))
    return false;

  if (! this->createScanLists(ctx))
    return false;

  if (! this->linkScanLists(ctx))
    return false;

  if (! this->createGPSSystems(ctx))
    return false;

  if (! this->linkChannels(ctx))
    return false;

  if (! this->linkGPSSystems(ctx))
    return false;

  return true;
}


void
D868UVCodeplug::allocateChannels() {
  /* Allocate channels */
  uint8_t *channel_bitmap = data(CHANNEL_BITMAP);
  for (uint16_t i=0; i<NUM_CHANNELS; i++) {
    // Get byte and bit for channel, as well as bank of channel
    uint16_t bit = i%8, byte = i/8, bank = i/128, idx=i%128;
    // if disabled -> skip
    if (0 == ((channel_bitmap[byte]>>bit) & 0x01))
      continue;
    // compute address for channel
    uint32_t addr = CHANNEL_BANK_0
        + bank*CHANNEL_BANK_OFFSET
        + idx*CHANNEL_SIZE;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, CHANNEL_SIZE);
    }
  }
}

bool
D868UVCodeplug::encodeChannels(const Flags &flags, Context &ctx) {
  // Encode channels
  for (int i=0; i<ctx.config()->channelList()->count(); i++) {
    // enable channel
    uint16_t bank = i/128, idx = i%128;
    ChannelElement ch(data(CHANNEL_BANK_0 + bank*CHANNEL_BANK_OFFSET + idx*CHANNEL_SIZE));
    if (! ch.fromChannelObj(ctx.config()->channelList()->channel(i), ctx))
      return false;
  }
  return true;
}

bool
D868UVCodeplug::createChannels(Context &ctx) {
  // Create channels
  uint8_t *channel_bitmap = data(CHANNEL_BITMAP);
  for (uint16_t i=0; i<NUM_CHANNELS; i++) {
    // Check if channel is enabled:
    uint16_t  bit = i%8, byte = i/8, bank = i/128, idx = i%128;
    if (0 == ((channel_bitmap[byte]>>bit) & 0x01))
      continue;
    ChannelElement ch(data(CHANNEL_BANK_0 + bank*CHANNEL_BANK_OFFSET + idx*CHANNEL_SIZE));
    if (Channel *obj = ch.toChannelObj(ctx))
      ctx.add(obj, i);
  }
  return true;
}

bool
D868UVCodeplug::linkChannels(Context &ctx) {
  // Link channel objects
  for (uint16_t i=0; i<NUM_CHANNELS; i++) {
    // Check if channel is enabled:
    uint16_t  bit = i%8, byte = i/8, bank = i/128, idx = i%128;
    if (0 == (((*data(CHANNEL_BITMAP+byte))>>bit) & 0x01))
      continue;
    ChannelElement ch(data(CHANNEL_BANK_0 + bank*CHANNEL_BANK_OFFSET + idx*CHANNEL_SIZE));
    if (ctx.has<Channel>(i))
      ch.linkChannelObj(ctx.get<Channel>(i), ctx);
  }
  return true;
}


void
D868UVCodeplug::allocateVFOSettings() {
  // Allocate VFO channels
  image(0).addElement(VFO_A_ADDR, CHANNEL_SIZE);
  image(0).addElement(VFO_A_ADDR+0x2000, CHANNEL_SIZE);
  image(0).addElement(VFO_B_ADDR, CHANNEL_SIZE);
  image(0).addElement(VFO_B_ADDR+0x2000, CHANNEL_SIZE);
}

void
D868UVCodeplug::allocateContacts() {
  /* Allocate contacts */
  uint8_t *contact_bitmap = data(CONTACTS_BITMAP);
  uint contactCount=0;
  for (uint16_t i=0; i<NUM_CONTACTS; i++) {
    // enabled if false (ass hole)
    if (1 == ((contact_bitmap[i/8]>>(i%8)) & 0x01))
      continue;
    contactCount++;
    uint32_t addr = CONTACT_BANK_0+(i/CONTACTS_PER_BANK)*CONTACT_BANK_SIZE;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, CONTACT_BANK_SIZE);
      memset(data(addr), 0x00, CONTACT_BANK_SIZE);
    }
  }
  if (contactCount) {
    image(0).addElement(CONTACT_INDEX_LIST, align_size(4*contactCount, 16));
    memset(data(CONTACT_INDEX_LIST), 0xff, align_size(4*contactCount, 16));
    image(0).addElement(CONTACT_ID_MAP, align_size(CONTACT_ID_ENTRY_SIZE*(1+contactCount), 16));
    memset(data(CONTACT_ID_MAP), 0xff, align_size(CONTACT_ID_ENTRY_SIZE*(1+contactCount), 16));
  }
}

bool
D868UVCodeplug::encodeContacts(const Flags &flags, Context &ctx) {
  QVector<DigitalContact*> contacts;
  // Encode contacts and also collect id<->index map
  for (int i=0; i<ctx.config()->contacts()->digitalCount(); i++) {
    ContactElement con(data(CONTACT_BANK_0+i*CONTACT_SIZE));
    DigitalContact *contact = ctx.config()->contacts()->digitalContact(i);
    if(! con.fromContactObj(contact, ctx))
      return false;
    ((uint32_t *)data(CONTACT_INDEX_LIST))[i] = qToLittleEndian(i);
    contacts.append(contact);
  }
  // encode index map for contacts
  std::sort(contacts.begin(), contacts.end(),
            [](DigitalContact *a, DigitalContact *b) {
    return a->number() < b->number();
  });
  for (int i=0; i<contacts.size(); i++) {
    ContactMapElement el(data(CONTACT_ID_MAP + i*CONTACT_ID_ENTRY_SIZE));
    el.setID(contacts[i]->number(), (DigitalContact::GroupCall==contacts[i]->type()));
    el.setIndex(ctx.index(contacts[i]));
  }
  return true;
}

bool
D868UVCodeplug::createContacts(Context &ctx) {
  // Create digital contacts
  uint8_t *contact_bitmap = data(CONTACTS_BITMAP);
  for (uint16_t i=0; i<NUM_CONTACTS; i++) {
    // Check if contact is enabled:
    uint16_t  bit = i%8, byte = i/8;
    if (1 == ((contact_bitmap[byte]>>bit) & 0x01))
      continue;
    contact_t *con = (contact_t *)data(CONTACT_BANK_0+i*sizeof(contact_t));
    if (DigitalContact *obj = con->toContactObj()) {
      ctx.config()->contacts()->add(obj); ctx.add(obj, i);
    }
  }
  return true;
}


void
D868UVCodeplug::allocateAnalogContacts() {
  /* Allocate analog contacts */
  uint8_t *analog_contact_bytemap = data(ANALOGCONTACT_BYTEMAP);
  for (uint8_t i=0; i<NUM_ANALOGCONTACTS; i+=2) {
    // if disabled -> skip
    if (0xff == analog_contact_bytemap[i])
      continue;
    uint32_t addr = ANALOGCONTACT_BANK_0 + (i/ANALOGCONTACTS_PER_BANK)*ANALOGCONTACT_BANK_SIZE;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, ANALOGCONTACT_BANK_SIZE);
    }
  }
  image(0).addElement(ANALOGCONTACT_INDEX_LIST, ANALOGCONTACT_LIST_SIZE);
}

bool
D868UVCodeplug::encodeAnalogContacts(const Flags &flags, Context &ctx) {
  uint8_t *idxlst = data(ANALOGCONTACT_INDEX_LIST);
  memset(idxlst, 0xff, ANALOGCONTACT_LIST_SIZE);
  for (int i=0; i<ctx.config()->contacts()->dtmfCount(); i++) {
    uint32_t addr = ANALOGCONTACT_BANK_0 + (i/ANALOGCONTACTS_PER_BANK)*ANALOGCONTACT_BANK_SIZE
        + (i%ANALOGCONTACTS_PER_BANK)*ANALOGCONTACT_SIZE;
    DTMFContactElement cont(data(addr));
    cont.fromContact(ctx.config()->contacts()->dtmfContact(i));
    idxlst[i] = i;
  }
  return true;
}

bool
D868UVCodeplug::createAnalogContacts(Context &ctx) {
  uint8_t *analog_contact_bytemap = data(ANALOGCONTACT_BYTEMAP);
  for (uint8_t i=0; i<NUM_ANALOGCONTACTS; i++) {
    // if disabled -> skip
    if (0xff == analog_contact_bytemap[i])
      continue;
    uint32_t addr = ANALOGCONTACT_BANK_0 + (i/ANALOGCONTACTS_PER_BANK)*ANALOGCONTACT_BANK_SIZE
        + (i%ANALOGCONTACTS_PER_BANK)*ANALOGCONTACT_SIZE;
    DTMFContactElement cont(data(addr));
    DTMFContact *dtmf = cont.toContact();
    ctx.config()->contacts()->add(dtmf); ctx.add(dtmf, i);
  }
  return true;
}


void
D868UVCodeplug::allocateRadioIDs() {
  /* Allocate radio IDs */
  uint8_t *radioid_bitmap = data(RADIOID_BITMAP);
  for (uint8_t i=0; i<NUM_RADIOIDS; i++) {
    // Get byte and bit for radio ID
    uint16_t bit = i%8, byte = i/8;
    // if disabled -> skip
    if (0 == ((radioid_bitmap[byte]>>bit) & 0x01))
      continue;
    // Allocate radio IDs individually
    uint32_t addr = ADDR_RADIOIDS + i*RADIOID_SIZE;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, RADIOID_SIZE);
    }
  }
}

bool
D868UVCodeplug::encodeRadioID(const Flags &flags, Context &ctx) {
  // Encode radio IDs
  for (int i=0; i<ctx.config()->radioIDs()->count(); i++) {
    RadioIDElement(data(ADDR_RADIOIDS + i*RADIOID_SIZE)).fromRadioID(
          ctx.config()->radioIDs()->getId(i));
  }
  return true;
}

bool
D868UVCodeplug::setRadioID(Context &ctx) {
  // Find a valid RadioID
  uint8_t *radio_id_bitmap = data(RADIOID_BITMAP);
  for (uint16_t i=0; i<NUM_RADIOIDS; i++) {
    if (0 == (radio_id_bitmap[i/8] & (1 << (i%8))))
      continue;
    RadioIDElement id(data(ADDR_RADIOIDS + i*RADIOID_SIZE));
    logDebug() << "Store id " << id.number() << " at idx " << i << ".";
    RadioID *rid = id.toRadioID();
    ctx.config()->radioIDs()->add(rid); ctx.add(rid, i);
  }
  return true;
}


void
D868UVCodeplug::allocateRXGroupLists() {
  /*
   * Allocate group lists
   */
  uint8_t *grouplist_bitmap = data(RXGRP_BITMAP);
  for (uint16_t i=0; i<NUM_RXGRP; i++) {
    // Get byte and bit for group list
    uint16_t bit = i%8, byte = i/8;
    // if disabled -> skip
    if (0 == ((grouplist_bitmap[byte]>>bit) & 0x01))
      continue;
    // Allocate RX group lists indivitually
    uint32_t addr = ADDR_RXGRP_0 + i*RXGRP_OFFSET;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, RXGRP_SIZE);
      memset(data(addr), 0xff, RXGRP_SIZE);
    }
  }

}

bool
D868UVCodeplug::encodeRXGroupLists(const Flags &flags, Context &ctx) {
  // Encode RX group-lists
  for (int i=0; i<ctx.config()->rxGroupLists()->count(); i++) {
    GroupListElement grp(data(ADDR_RXGRP_0 + i*RXGRP_OFFSET));
    grp.fromGroupListObj(ctx.config()->rxGroupLists()->list(i), ctx);
  }
  return true;
}

bool
D868UVCodeplug::createRXGroupLists(Context &ctx) {
  // Create RX group lists
  uint8_t *grouplist_bitmap = data(RXGRP_BITMAP);
  for (uint16_t i=0; i<NUM_RXGRP; i++) {
    // check if group list is enabled
    uint16_t  bit = i%8, byte = i/8;
    if (0 == ((grouplist_bitmap[byte]>>bit) & 0x01))
      continue;
    // construct RXGroupList from definition
    GroupListElement grp(data(ADDR_RXGRP_0+i*RXGRP_OFFSET));
    if (RXGroupList *obj = grp.toGroupListObj()) {
      ctx.config()->rxGroupLists()->add(obj); ctx.add(obj, i);
    }
  }
  return true;
}

bool
D868UVCodeplug::linkRXGroupLists(Context &ctx) {
  uint8_t *grouplist_bitmap = data(RXGRP_BITMAP);
  for (uint16_t i=0; i<NUM_RXGRP; i++) {
    // check if group list is enabled
    uint16_t  bit = i%8, byte = i/8;
    if (0 == ((grouplist_bitmap[byte]>>bit) & 0x01))
      continue;
    // link group list
    GroupListElement grp(data(ADDR_RXGRP_0+i*RXGRP_OFFSET));
    if (! grp.linkGroupList(ctx.get<RXGroupList>(i), ctx)) {
      logError() << "Cannot link RX group list idx=" << i;
      return false;
    }
  }
  return true;
}


void
D868UVCodeplug::allocateZones() {
  uint8_t *zone_bitmap = data(ZONE_BITMAPS);
  for (uint16_t i=0; i<NUM_ZONES; i++) {
    // Get byte and bit for zone
    uint16_t bit = i%8, byte = i/8;
    // if invalid -> skip
    if (0 == ((zone_bitmap[byte]>>bit) & 0x01))
      continue;
    // Allocate zone itself
    image(0).addElement(ADDR_ZONE+i*ZONE_OFFSET, ZONE_SIZE);
    image(0).addElement(ADDR_ZONE_NAME+i*ZONE_NAME_OFFSET, ZONE_NAME_SIZE);
  }
}

bool
D868UVCodeplug::encodeZones(const Flags &flags, Context &ctx) {
  // Encode zones
  uint zidx = 0;
  for (int i=0; i<ctx.config()->zones()->count(); i++) {
    // Clear name and channel list
    uint8_t  *name     = (uint8_t *)data(ADDR_ZONE_NAME + zidx*ZONE_NAME_OFFSET);
    uint16_t *channels = (uint16_t *)data(ADDR_ZONE + zidx*ZONE_OFFSET);
    memset(name, 0, ZONE_NAME_SIZE);
    memset(channels, 0xff, ZONE_SIZE);
    if (ctx.config()->zones()->zone(i)->B()->count())
      encode_ascii(name, ctx.config()->zones()->zone(i)->name()+" A", 16, 0);
    else
      encode_ascii(name, ctx.config()->zones()->zone(i)->name(), 16, 0);
    // Handle list A
    for (int j=0; j<ctx.config()->zones()->zone(i)->A()->count(); j++) {
      channels[j] = qToLittleEndian(ctx.index(ctx.config()->zones()->zone(i)->A()->get(j)));
    }
    zidx++;
    if (! ctx.config()->zones()->zone(i)->B()->count())
      continue;

    // Process list B if present
    name     = (uint8_t *)data(ADDR_ZONE_NAME+zidx*ZONE_NAME_OFFSET);
    channels = (uint16_t *)data(ADDR_ZONE+zidx*ZONE_OFFSET);
    memset(name, 0, ZONE_NAME_SIZE);
    memset(channels, 0xff, ZONE_SIZE);
    encode_ascii(name, ctx.config()->zones()->zone(i)->name()+" B", 16, 0);
    // Handle list B
    for (int j=0; j<ctx.config()->zones()->zone(i)->B()->count(); j++) {
      channels[j] = qToLittleEndian(ctx.index(ctx.config()->zones()->zone(i)->B()->get(j)));
    }
    zidx++;
  }
  return true;
}

bool
D868UVCodeplug::createZones(Context &ctx) {
  // Create zones
  uint8_t *zone_bitmap = data(ZONE_BITMAPS);
  QString last_zonename, last_zonebasename; Zone *last_zone = nullptr;
  bool extend_last_zone = false;
  for (uint16_t i=0; i<NUM_ZONES; i++) {
    // Check if zone is enabled:
    uint16_t bit = i%8, byte = i/8;
    if (0 == ((zone_bitmap[byte]>>bit) & 0x01))
      continue;
    // Determine whether this zone should be combined with the previous one
    QString zonename = decode_ascii(data(ADDR_ZONE_NAME+i*ZONE_NAME_OFFSET), 16, 0);
    QString zonebasename = zonename; zonebasename.chop(2);
    extend_last_zone = ( zonename.endsWith(" B") && last_zonename.endsWith(" A")
                         && (zonebasename == last_zonebasename)
                         && (nullptr != last_zone) && (0 == last_zone->B()->count()) );
    last_zonename = zonename;
    last_zonebasename = zonebasename;

    // If enabled, create zone with name
    if (! extend_last_zone) {
      last_zone = new Zone(zonename);
      // add to config
      ctx.config()->zones()->add(last_zone);
    } else {
      // when extending the last zone, chop its name to remove the "... A" part.
      last_zone->setName(last_zonebasename);
    }

    // link zone
    uint16_t *channels = (uint16_t *)data(ADDR_ZONE+i*ZONE_OFFSET);
    for (uint8_t i=0; i<NUM_CH_PER_ZONE; i++, channels++) {
      // If not enabled -> continue
      if (0xffff == *channels)
        continue;
      // Get channel index and check if defined
      uint16_t cidx = qFromLittleEndian(*channels);
      if (! ctx.has<Channel>(cidx))
        continue;
      // If defined -> add channel to zone obj
      if (extend_last_zone)
        last_zone->B()->add(ctx.get<Channel>(cidx));
      else
        last_zone->A()->add(ctx.get<Channel>(cidx));
    }
  }
  return true;
}

bool
D868UVCodeplug::linkZones(Context &ctx) {
  // Create zones
  uint8_t *zone_bitmap = data(ZONE_BITMAPS);
  QString last_zonename, last_zonebasename; Zone *last_zone = nullptr;
  bool extend_last_zone = false;
  for (uint16_t i=0; i<NUM_ZONES; i++) {
    // Check if zone is enabled:
    uint16_t bit = i%8, byte = i/8;
    if (0 == ((zone_bitmap[byte]>>bit) & 0x01))
      continue;
    // Determine whether this zone should be combined with the previous one
    QString zonename = decode_ascii(data(ADDR_ZONE_NAME+i*ZONE_NAME_OFFSET), 16, 0);
    QString zonebasename = zonename; zonebasename.chop(2);
    extend_last_zone = ( zonename.endsWith(" B") && last_zonename.endsWith(" A")
                         && (zonebasename == last_zonebasename)
                         && (nullptr != last_zone) && (0 == last_zone->B()->count()) );
    last_zonename = zonename;
    last_zonebasename = zonebasename;

    // If enabled, get zone
    if (! extend_last_zone) {
      last_zone = ctx.get<Zone>(i);
    } else {
      // when extending the last zone, chop its name to remove the "... A" part.
      last_zone->setName(last_zonebasename);
    }

    // link zone
    uint16_t *channels = (uint16_t *)data(ADDR_ZONE+i*ZONE_OFFSET);
    for (uint8_t i=0; i<NUM_CH_PER_ZONE; i++, channels++) {
      // If not enabled -> continue
      if (0xffff == *channels)
        continue;
      // Get channel index and check if defined
      uint16_t cidx = qFromLittleEndian(*channels);
      if (! ctx.has<Channel>(cidx))
        continue;
      // If defined -> add channel to zone obj
      if (extend_last_zone)
        last_zone->B()->add(ctx.get<Channel>(cidx));
      else
        last_zone->A()->add(ctx.get<Channel>(cidx));
    }
  }
  return true;
}


void
D868UVCodeplug::allocateScanLists() {
  /*
   * Allocate scan lists
   */
  uint8_t *scanlist_bitmap = data(SCAN_BITMAP);
  for (uint8_t i=0; i<NUM_SCAN_LISTS; i++) {
    // Get byte and bit for scan list, bank and bank_idx
    uint16_t bit = i%8, byte = i/8;
    uint8_t bank = (i/NUM_SCANLISTS_PER_BANK), bank_idx = (i%NUM_SCANLISTS_PER_BANK);
    // if disabled -> skip
    if (0 == ((scanlist_bitmap[byte]>>bit) & 0x01))
      continue;
    // Allocate scan lists indivitually
    uint32_t addr = SCAN_LIST_BANK_0 + bank*SCAN_LIST_BANK_OFFSET + bank_idx*SCAN_LIST_OFFSET;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, SCAN_LIST_SIZE);
      memset(data(addr), 0xff, SCAN_LIST_SIZE);
    }
  }
}

bool
D868UVCodeplug::encodeScanLists(const Flags &flags, Context &ctx) {
  // Encode scan lists
  for (int i=0; i<ctx.config()->scanlists()->count(); i++) {
    uint8_t bank = i/NUM_SCANLISTS_PER_BANK, idx = i%NUM_SCANLISTS_PER_BANK;
    ScanListElement scan(data(SCAN_LIST_BANK_0 + bank*SCAN_LIST_BANK_OFFSET + idx*SCAN_LIST_OFFSET));
    scan.fromScanListObj(ctx.config()->scanlists()->scanlist(i), ctx);
  }
  return true;
}

bool
D868UVCodeplug::createScanLists(Context &ctx) {
  // Create scan lists
  uint8_t *scanlist_bitmap = data(SCAN_BITMAP);
  for (uint i=0; i<NUM_SCAN_LISTS; i++) {
    uint8_t byte=i/8, bit=i%8;
    if (0 == ((scanlist_bitmap[byte]>>bit) & 0x01))
      continue;
    uint8_t bank = i/NUM_SCANLISTS_PER_BANK, bank_idx = i%NUM_SCANLISTS_PER_BANK;
    uint32_t addr = SCAN_LIST_BANK_0 + bank*SCAN_LIST_BANK_OFFSET + bank_idx*SCAN_LIST_OFFSET;
    ScanListElement scanl(data(addr));
    // Create scanlist
    ScanList *obj = scanl.toScanListObj();
    ctx.config()->scanlists()->add(obj); ctx.add(obj, i);
  }
  return true;
}

bool
D868UVCodeplug::linkScanLists(Context &ctx) {
  uint8_t *scanlist_bitmap = data(SCAN_BITMAP);
  for (uint i=0; i<NUM_SCAN_LISTS; i++) {
    uint8_t byte=i/8, bit=i%8;
    if (0 == ((scanlist_bitmap[byte]>>bit) & 0x01))
      continue;
    uint8_t bank = i/NUM_SCANLISTS_PER_BANK, bank_idx = i%NUM_SCANLISTS_PER_BANK;
    uint32_t addr = SCAN_LIST_BANK_0 + bank*SCAN_LIST_BANK_OFFSET + bank_idx*SCAN_LIST_OFFSET;
    ScanListElement scanl(data(addr));
    // Create scanlist
    ScanList *obj = ctx.get<ScanList>(i);
    // Link scanlists immediately, all channels are defined allready
    ctx.config()->scanlists()->add(obj); scanl.linkScanListObj(obj, ctx);
  }
  return true;
}


void
D868UVCodeplug::allocateGeneralSettings() {
  image(0).addElement(ADDR_GENERAL_CONFIG, GENERAL_CONFIG_SIZE);
}

bool
D868UVCodeplug::encodeGeneralSettings(const Flags &flags, Context &ctx) {
  return GeneralSettingsElement(data(ADDR_GENERAL_CONFIG)).fromConfig(flags, ctx);
}

bool
D868UVCodeplug::decodeGeneralSettings(Context &ctx) {
  return GeneralSettingsElement(data(ADDR_GENERAL_CONFIG)).updateConfig(ctx);
}


void
D868UVCodeplug::allocateZoneChannelList() {
  image(0).addElement(ADDR_ZONE_CHANNELS, ZONE_CHANNELS_SIZE);
}


void
D868UVCodeplug::allocateDTMFNumbers() {
  image(0).addElement(ADDR_DTMF_NUMBERS, DTMF_NUMBERS_SIZE);
}


void
D868UVCodeplug::allocateBootSettings() {
  image(0).addElement(ADDR_BOOT_SETTINGS, BOOT_SETTINGS_SIZE);
}

bool
D868UVCodeplug::encodeBootSettings(const Flags &flags, Context &ctx) {
  return BootSettingsElement(data(ADDR_BOOT_SETTINGS)).fromConfig(flags, ctx);
}

bool
D868UVCodeplug::decodeBootSettings(Context &ctx) {
  return BootSettingsElement(data(ADDR_BOOT_SETTINGS)).updateConfig(ctx);
}


void
D868UVCodeplug::allocateGPSSystems() {
  image(0).addElement(ADDR_GPS_SETTINGS, GPS_SETTINGS_SIZE);
  image(0).addElement(ADDR_GPS_MESSAGE, GPS_MESSAGE_SIZE);
}

bool
D868UVCodeplug::encodeGPSSystems(const Flags &flags, Context &ctx) {
  DMRAPRSSettingsElement gps(data(ADDR_GPS_SETTINGS));
  return gps.fromConfig(flags, ctx);
}

bool
D868UVCodeplug::createGPSSystems(Context &ctx) {
  QSet<uint8_t> systems;
  // First find all GPS systems linked, that is referenced by any channel
  // Create channels
  uint8_t *channel_bitmap = data(CHANNEL_BITMAP);
  for (uint16_t i=0; i<NUM_CHANNELS; i++) {
    // Check if channel is enabled:
    uint16_t  bit = i%8, byte = i/8, bank = i/128, idx = i%128;
    if (0 == ((channel_bitmap[byte]>>bit) & 0x01))
      continue;
    if (ctx.get<Channel>(i)->is<AnalogChannel>())
      continue;
    ChannelElement ch(data(CHANNEL_BANK_0 + bank*CHANNEL_BANK_OFFSET + idx*CHANNEL_SIZE));
    if (ch.txDigitalAPRS())
      systems.insert(ch.digitalAPRSSystemIndex());
  }
  // Then create all referenced GPS systems
  DMRAPRSSettingsElement gps(data(ADDR_GPS_SETTINGS));
  for (QSet<uint8_t>::iterator idx=systems.begin(); idx!=systems.end(); idx++)
    gps.createGPSSystem(*idx, ctx);
  return true;
}

bool
D868UVCodeplug::linkGPSSystems(Context &ctx) {
  DMRAPRSSettingsElement gps(data(ADDR_GPS_SETTINGS));
  // Then link all referenced GPS systems
  for (uint8_t i=0; i<8; i++) {
    if (! ctx.has<GPSSystem>(i))
      continue;
    gps.linkGPSSystem(i, ctx);
  }
  return true;
}


void
D868UVCodeplug::allocateSMSMessages() {
  // Prefab. SMS messages
  uint8_t *messages_bytemap = data(MESSAGE_BYTEMAP);
  uint message_count = 0;
  for (uint8_t i=0; i<NUM_MESSAGES; i++) {
    uint8_t bank = i/NUM_MESSAGES_PER_BANK;
    if (0xff == messages_bytemap[i])
      continue;
    message_count++;
    uint32_t addr = MESSAGE_BANK_0 + bank*MESSAGE_BANK_SIZE;
    if (nullptr == data(addr, 0)) {
      image(0).addElement(addr, MESSAGE_BANK_SIZE);
    }
  }
  if (message_count) {
    image(0).addElement(MESSAGE_INDEX_LIST, 0x10*message_count);
  }
}

void
D868UVCodeplug::allocateHotKeySettings() {
  // Allocate Hot Keys
  image(0).addElement(ADDR_HOTKEY, HOTKEY_SIZE);
}

void
D868UVCodeplug::allocateRepeaterOffsetSettings() {
  // Offset frequencies
  image(0).addElement(ADDR_OFFSET_FREQ, OFFSET_FREQ_SIZE);
}

void
D868UVCodeplug::allocateAlarmSettings() {
  // Alarm settings
  image(0).addElement(ADDR_ALARM_SETTING, ALARM_SETTING_SIZE);
  image(0).addElement(ADDR_ALARM_SETTING_EXT, ALARM_SETTING_EXT_SIZE);
}

void
D868UVCodeplug::allocateFMBroadcastSettings() {
  // FM broad-cast settings
  image(0).addElement(ADDR_FMBC, FMBC_SIZE+FMBC_VFO_SIZE);
}

void
D868UVCodeplug::allocate5ToneIDs() {
  // Allocate 5-tone functions
  uint8_t *bitmap = data(FIVE_TONE_ID_BITMAP);
  for (uint8_t i=0; i<NUM_FIVE_TONE_IDS; i++) {
    uint16_t  bit = i%8, byte = i/8;
    if (0 == (bitmap[byte] & (1<<bit)))
      continue;
    image(0).addElement(ADDR_FIVE_TONE_ID_LIST + i*FIVE_TONE_ID_SIZE, FIVE_TONE_ID_SIZE);
  }
}

void
D868UVCodeplug::allocate5ToneFunctions() {
  image(0).addElement(ADDR_FIVE_TONE_FUNCTIONS, FIVE_TONE_FUNCTIONS_SIZE);
}

void
D868UVCodeplug::allocate5ToneSettings() {
  image(0).addElement(ADDR_FIVE_TONE_SETTINGS, FIVE_TONE_SETTINGS_SIZE);
}

void
D868UVCodeplug::allocate2ToneIDs() {
  // Allocate 2-tone encoding
  uint8_t *enc_bitmap = data(TWO_TONE_IDS_BITMAP);
  for (uint8_t i=0; i<NUM_TWO_TONE_IDS; i++) {
    uint16_t  bit = i%8, byte = i/8;
    if (0 == (enc_bitmap[byte] & (1<<bit)))
      continue;
    image(0).addElement(ADDR_TWO_TONE_IDS + i*TWO_TONE_ID_SIZE, TWO_TONE_ID_SIZE);
  }
}


void
D868UVCodeplug::allocate2ToneFunctions() {
  // Allocate 2-tone decoding
  uint8_t *dec_bitmap = data(TWO_TONE_FUNCTIONS_BITMAP);
  for (uint8_t i=0; i<NUM_TWO_TONE_FUNCTIONS; i++) {
    uint16_t  bit = i%8, byte = i/8;
    if (0 == (dec_bitmap[byte] & (1<<bit)))
      continue;
    image(0).addElement(ADDR_TWO_TONE_FUNCTIONS + i*TWO_TONE_FUNCTION_SIZE, TWO_TONE_FUNCTION_SIZE);
  }
}

void
D868UVCodeplug::allocate2ToneSettings() {
  image(0).addElement(ADDR_TWO_TONE_SETTINGS, TWO_TONE_SETTINGS_SIZE);
}


void
D868UVCodeplug::allocateDTMFSettings() {
  image(0).addElement(ADDR_DTMF_SETTINGS, DTMF_SETTINGS_SIZE);
}
