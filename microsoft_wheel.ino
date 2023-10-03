#include <usb/usb_host.h>
#include "show_desc.hpp"
#include "usbhhelp.hpp"

bool isWHEEL = false;
bool isWHEELReady = false;

const size_t WHEEL_IN_BUFFERS = 1;
const size_t WHEEL_OUT_BUFFERS = 1;
usb_transfer_t *WHEELOut = NULL;
usb_transfer_t *WHEELIn[WHEEL_IN_BUFFERS] = {NULL};

void prepare_endpoints(const void *p)
{
  const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)p;
  esp_err_t err;
  
  if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
    ESP_LOGI("", "bEndpointAddress: 0x%02x", endpoint->bEndpointAddress);
    for (int i = 0; i < WHEEL_IN_BUFFERS; i++) {
      err = usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0, &WHEELIn[i]);
      if (err != ESP_OK) {
        WHEELIn[i] = NULL;
        ESP_LOGI("", "usb_host_transfer_alloc In fail: %x", err);
      } else {
        WHEELIn[i]->device_handle = Device_Handle;
        WHEELIn[i]->bEndpointAddress = endpoint->bEndpointAddress;
        WHEELIn[i]->callback = wheel_transfer_cb;
        WHEELIn[i]->context = (void *)i;
        WHEELIn[i]->num_bytes = endpoint->wMaxPacketSize;
        esp_err_t err = usb_host_transfer_submit(WHEELIn[i]);
        if (err != ESP_OK) {
          ESP_LOGI("", "usb_host_transfer_submit In fail: %x", err);
        }
      }
    }
  } else {
    err = usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0, &WHEELOut);
    if (err != ESP_OK) {
      WHEELOut = NULL;
      ESP_LOGI("", "usb_host_transfer_alloc Out fail: %x", err);
      return;
    }
    ESP_LOGI("", "Out data_buffer_size: %d", WHEELOut->data_buffer_size);
    WHEELOut->device_handle = Device_Handle;
    WHEELOut->bEndpointAddress = endpoint->bEndpointAddress;
    WHEELOut->callback = wheel_transfer_cb;
    WHEELOut->context = NULL;
//    WHEELOut->flags |= USB_TRANSFER_FLAG_ZERO_PACK;
  }
  
  isWHEELReady = ((WHEELOut != NULL) && (WHEELIn[0] != NULL));
}


static void wheel_transfer_cb(usb_transfer_t *transfer)
{
  if (Device_Handle == transfer->device_handle) {
    int in_xfer = transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;
    if ((transfer->status == 0) && in_xfer) {
      uint8_t *const p = transfer->data_buffer;
      // for (int i = 0; i < transfer->actual_num_bytes; i ++) Serial.printf("%02X ", p[i]);
      int16_t wheel = ((p[1] & 0x03) << 8) + p[0]; if(wheel & 0x0200) wheel = wheel | 0xFC00;
      Serial.printf("[ Wheel:%03d ",wheel);
      Serial.printf(" Left:%02d ",63 - (p[2] & 0x7F) );
      Serial.printf(" Right:%02d ",63 - (p[1] >> 2) );
      if(p[3] & 0x01) Serial.printf(" A "); else Serial.printf(" _ ");
      if(p[3] & 0x02) Serial.printf(" B "); else Serial.printf(" _ ");
      if(p[3] & 0x04) Serial.printf(" C "); else Serial.printf(" _ ");
      if(p[3] & 0x08) Serial.printf(" X "); else Serial.printf(" _ ");
      if(p[3] & 0x10) Serial.printf(" Y "); else Serial.printf(" _ ");
      if(p[3] & 0x20) Serial.printf(" Z "); else Serial.printf(" _ ");
      if(p[3] & 0x40) Serial.printf(" < "); else Serial.printf(" _ ");
      if(p[3] & 0x80) Serial.printf(" > "); else Serial.printf(" _ ");    
      Serial.println("]");

      esp_err_t err = usb_host_transfer_submit(transfer);
      if (err != ESP_OK) ESP_LOGI("", "usb_host_transfer_submit In fail: %x", err);
    }
    else ESP_LOGI("", "transfer->status %d", transfer->status);
  }
}


// ----------------------------------------------------------

void show_config_desc_full(const usb_config_desc_t *config_desc)
{
  // Full decode of config desc.
  const uint8_t *p = &config_desc->val[0];
  static uint8_t USB_Class = 0;
  uint8_t bLength;
  for (int i = 0; i < config_desc->wTotalLength; i+=bLength, p+=bLength) {
    ESP_LOGI("", "==========> part %d",i);   
    bLength = *p;
    if ((i + bLength) <= config_desc->wTotalLength) {
      const uint8_t bDescriptorType = *(p + 1);
      switch (bDescriptorType) {
        case USB_B_DESCRIPTOR_TYPE_DEVICE: ESP_LOGI("", "USB Device Descriptor should not appear in config"); break;
        case USB_B_DESCRIPTOR_TYPE_CONFIGURATION: show_config_desc(p); break;
        case USB_B_DESCRIPTOR_TYPE_STRING: ESP_LOGI("", "USB string desc TBD"); break;

        case USB_B_DESCRIPTOR_TYPE_INTERFACE:
          ESP_LOGI("", "-> USB INTERFACE");
          USB_Class = show_interface_desc(p);
          if (!isWHEEL) {
                const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
                if ((intf->bInterfaceClass == USB_CLASS_HID) && (intf->bInterfaceSubClass == 0) && (intf->bInterfaceProtocol == 0)) {
                    isWHEEL = true;
                    ESP_LOGI("", "##################### WHEEL device found! #######################");
                    esp_err_t err = usb_host_interface_claim(Client_Handle, Device_Handle, intf->bInterfaceNumber, intf->bAlternateSetting);
                    if (err != ESP_OK) ESP_LOGI("", "usb_host_interface_claim failed: %x", err);
                } else ESP_LOGI("", "Unknoun Wheel device bInterfaceClass=[%d] bInterfaceSubClass=[%d] bInterfaceProtocol=[%d]", intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);
          }
          break;

        case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
          ESP_LOGI("", "-> USB ENDPOINT");
          show_endpoint_desc(p);
          if (isWHEEL && !isWHEELReady) prepare_endpoints(p);
          break;
   
        case USB_B_DESCRIPTOR_TYPE_DEVICE_QUALIFIER: ESP_LOGI("", "USB device qual desc TBD"); break;
        case USB_B_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION: ESP_LOGI("", "USB Other Speed TBD"); break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE_POWER: ESP_LOGI("", "USB Interface Power TBD"); break;
        case USB_B_DESCRIPTOR_TYPE_OTG: ESP_LOGI("", "USB OTG TBD"); break;
        case USB_B_DESCRIPTOR_TYPE_DEBUG: ESP_LOGI("", "USB DEBUG TBD"); break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
          ESP_LOGI("", "-> USB INTERFACE_ASSOCIATION");
          show_interface_assoc(p);
          break;
        // Class specific descriptors have overlapping values.
        case 0x21:
          switch (USB_Class) {
            case USB_CLASS_HID: show_hid_desc(p); break;
            case USB_CLASS_APP_SPEC: ESP_LOGI("", "App Spec Class descriptor TBD"); break;
            default: ESP_LOGI("", "Unknown USB Descriptor Type: 0x%02x Class: 0x%02x", bDescriptorType, USB_Class); break;
          }
          break;
        case 0x22:
          switch (USB_Class) {
            default: ESP_LOGI("", "Unknown USB Descriptor Type: 0x%02x Class: 0x%02x", bDescriptorType, USB_Class); break;
          }
          break;
        case 0x23:
          switch (USB_Class) {
              default: ESP_LOGI("", "Unknown USB Descriptor Type: 0x%02x Class: 0x%02x", bDescriptorType, USB_Class); break;
          }
          break;
        case 0x24:
          switch (USB_Class) {
            case USB_CLASS_AUDIO: ESP_LOGI("", "Audio Class Descriptor 0x24 TBD"); break;
            case USB_CLASS_COMM: ESP_LOGI("", "Comm Class CS_INTERFACE 0x24 TBD"); break;
            default: ESP_LOGI("", "Unknown USB Descriptor Type: 0x%02x Class: 0x%02x", bDescriptorType, USB_Class); break;
          }
          break;
        case 0x25:
          switch (USB_Class) {
            case USB_CLASS_AUDIO: ESP_LOGI("", "Audio Class Descriptor 0x25 TBD"); break;
            case USB_CLASS_COMM: ESP_LOGI("", "Comm Class CS_ENDPOINT 0x25 TBD"); break;
            default: ESP_LOGI("", "Unknown USB Descriptor Type: 0x%02x Class: 0x%02x", bDescriptorType, USB_Class); break;
          }
          break;
        default: ESP_LOGI("", "Unknown USB Descriptor Type: 0x%02x Class: 0x%02x", bDescriptorType, USB_Class); break;
      }
    }
    else { ESP_LOGI("", "USB Descriptor invalid"); return; }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(0);
  usbh_setup(show_config_desc_full);
  Serial.printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("This chip has %d cores\n", ESP.getChipCores());
}

void loop()
{
  usbh_task();
}
