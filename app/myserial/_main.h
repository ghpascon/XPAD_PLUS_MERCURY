#include "vars.h"
#include "ble.h"

class MySerial
{
public:
    void setup()
    {
        Serial.begin(115200);

        setup_bt();

        my_keyboard.begin();
        USB.VID(0x03eb);
        USB.PID(0x2000);
        USB.manufacturerName("Smartx");
        USB.productName("XPAD_PLUS");
        USB.usbAttributes(0x80);
        USB.begin();
        my_usb.begin(115200);
    }

    void loop()
    {
        loop_bt();
        eth_state_changed();
    }

    void eth_state_changed()
    {
        static String last_state = "";
        if (eth_state != last_state)
        {
            write("Ethernet state: " + eth_state);
            if (eth_state == "got_ip" && pServer != nullptr)
            {
                stop_bt();
                write("BLE stopped due to Ethernet connection");
            }
            last_state = eth_state;
        }
    }

    void write(const String &data, bool all = false, bool bytes = false)
    {
        Serial.println(data);

        if (!all && simple_send)
            return;

        if (eth_connected)
            connection.telnet_write(data);
        if (btConnected)
            write_bt(data);

        if (keyboard)
        {
            if (btConnected)
                return;
            int interval = 10;
            for (size_t i = 0; i < data.length(); i++)
            {
                my_keyboard.write(data[i]);
                delay(interval);
                yield();
            }
            my_keyboard.write('\n');
            delay(interval);
            esp_task_wdt_reset();
        }
        else
        {
            if (bytes)
            {
                // Envio de bytes reais: data contém os bytes já prontos
                my_usb.write((const uint8_t *)data.c_str(), data.length());
            }
        }
    }

    void write_bytes(String data)
    {
        // Remove espaços, se houver
        data.replace(" ", "");
        for (size_t i = 0; i < data.length(); i += 2)
        {
            if (i + 1 < data.length())
            {
                String byteStr = data.substring(i, i + 2);
                int byteVal = (int)strtol(byteStr.c_str(), nullptr, 16);
                my_usb.write((uint8_t)byteVal);
            }
        }
    }

    String readLine(Stream &stream)
    {
        String cmd = "";
        unsigned long lastRead = millis();
        while (true)
        {
            if (stream.available())
            {
                int b = stream.read();
                if (b < 16)
                    cmd += "0";
                cmd += String(b, HEX);
                lastRead = millis();
            }
            else
            {
                if (cmd.length() > 0 && millis() - lastRead > 10)
                    break;
            }
            yield();
        }
        return cmd;
    }

    String check_serial()
    {
        String cmd = "";

        if (Serial.available())
        {
            cmd = readLine(Serial);
        }
        else if (my_usb.available())
        {
            cmd = readLine(my_usb);
        }
        else if (bt_cmd != "")
        {
            cmd = bt_cmd;
            bt_cmd = "";
        }
        else
        {
            cmd = connection.check_telnet();
        }
        cmd.toLowerCase();
        cmd.replace("  ", " ");

        return cmd;
    }

    // Função para calcular CRC8 (equivalente ao Python)
    uint16_t calc_crc8(uint16_t beginner, uint8_t ch)
    {
        uint8_t element80 = 0x80;
        for (int i = 0; i < 8; i++)
        {
            uint8_t xor_flag = (beginner >> 15) & 1;
            beginner = (beginner << 1) & 0xFFFF;
            if (ch & element80)
                beginner++;
            if (xor_flag)
                beginner ^= 0x1021;
            element80 >>= 1;
        }
        return beginner;
    }

    // Função para calcular o CRC do comando (equivalente ao calc_crc do Python)
    String get_crc(const String &hex)
    {
        // Remove espaços e converte para bytes
        String clean = hex;
        clean.replace(" ", "");
        int len = clean.length() / 2;
        if (len < 3)
            return ""; // Precisa de pelo menos 3 bytes
        uint8_t cmd[len];
        for (int i = 0; i < len; i++)
        {
            String byteStr = clean.substring(i * 2, i * 2 + 2);
            cmd[i] = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
        }

        uint16_t tempcalcCRC1 = calc_crc8(0xFFFF, cmd[1]);
        tempcalcCRC1 = calc_crc8(tempcalcCRC1, cmd[2]);
        if (cmd[1] != 0)
        {
            for (int i = 0; i < cmd[1]; i++)
                tempcalcCRC1 = calc_crc8(tempcalcCRC1, cmd[3 + i]);
        }
        // Retorna CRC em string hexadecimal (2 bytes, big endian)
        char crc_str[5];
        sprintf(crc_str, "%02X%02X", (tempcalcCRC1 >> 8) & 0xFF, tempcalcCRC1 & 0xFF);
        return String(crc_str);
    }

    // Função para calcular o CRC do retorno (equivalente ao calc_return_crc do Python)
    String get_return_crc(const String &hex)
    {
        // Remove espaços e converte para bytes
        String clean = hex;
        clean.replace(" ", "");
        int len = clean.length() / 2;
        if (len < 3)
            return ""; // Precisa de pelo menos 3 bytes
        uint8_t cmd[len];
        for (int i = 0; i < len; i++)
        {
            String byteStr = clean.substring(i * 2, i * 2 + 2);
            cmd[i] = (uint8_t)strtol(byteStr.c_str(), nullptr, 16);
        }

        uint16_t tempcalcCRC1 = calc_crc8(0xFFFF, cmd[1]);
        tempcalcCRC1 = calc_crc8(tempcalcCRC1, cmd[2]);
        for (int i = 0; i < (cmd[1] + 2); i++)
            tempcalcCRC1 = calc_crc8(tempcalcCRC1, cmd[3 + i]);
        // Retorna CRC em string hexadecimal (2 bytes, big endian)
        char crc_str[5];
        sprintf(crc_str, "%02X%02X", (tempcalcCRC1 >> 8) & 0xFF, tempcalcCRC1 & 0xFF);
        return String(crc_str);
    }
};
