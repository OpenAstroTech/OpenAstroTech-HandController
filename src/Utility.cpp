#include <Arduino.h>
#include <WString.h>

#include "Utility.hpp"

#if DEBUG_LEVEL > 0
    #include <stdarg.h>
#endif


int freeMemory()
{
    return ESP.getFreeHeap();
}

#if DEBUG_LEVEL > 0

String formatArg(const char *input, va_list args)
{
    const char *nibble = "0123456789ABCDEF";
    char achBuffer[255];
    char *p = achBuffer;

    for (const char *i = input; *i != 0; i++)
    {
        if (*i != '%')
        {
            *p++ = *i;
            continue;
        }
        i++;
        switch (*i)
        {
            case '%':
                {
                    *p++ = '%';
                }
                break;

            case 'c':
                {
                    char ch = (char) va_arg(args, int);
                    *p++    = ch;
                }
                break;

            case 's':
                {
                    char *s = va_arg(args, char *);
                    strcpy(p, s);
                    p += strlen(s);
                }
                break;

            case 'd':
                {
                    String s = String((int) va_arg(args, int));
                    strcpy(p, s.c_str());
                    p += s.length();
                }
                break;

            case 'x':
                {
                    int n             = (int) va_arg(args, int);
                    int shift         = 12;
                    unsigned int mask = 0xF000;
                    *p++              = '0';
                    *p++              = 'x';
                    while (shift >= 0)
                    {
                        int d = (n & mask) >> shift;
                        *p++  = *(nibble + d);
                        mask  = mask >> 4;
                        shift -= 4;
                    }

                    *p = 0;
                }
                break;

            case 'l':
                {
                    String s = String((long) va_arg(args, long));
                    strcpy(p, s.c_str());
                    p += s.length();
                }
                break;

            case 'f':
                {
                    float num = (float) va_arg(args, double);
                    String s  = String(num, 4);
                    strcpy(p, s.c_str());
                    p += s.length();
                }
                break;

            default:
                {
                    *p++ = *i;
                }
                break;
        }
    }

    *p = '\0';
    return String(achBuffer);
}

String format(const char *input, ...)
{
    va_list argp;
    va_start(argp, input);
    String ret = formatArg(input, argp);
    va_end(argp);
    return ret;
}

unsigned long lastLog = 0;

void logv(int levelFlags, String input, ...)
{
    if ((levelFlags & DEBUG_LEVEL) != 0)
    {
        unsigned long now   = millis();
        unsigned long delta = now - lastLog;
        va_list argp;
        va_start(argp, input);
    #if BUFFER_LOGS == true
        addToLogBuffer(formatArg(input.c_str(), argp));
    #else
        DEBUG_SERIAL_PORT.print("[");
        DEBUG_SERIAL_PORT.print(String(now));
        DEBUG_SERIAL_PORT.print("]{");
        DEBUG_SERIAL_PORT.print(String(delta));
        DEBUG_SERIAL_PORT.print("}ms:");
        DEBUG_SERIAL_PORT.print(String(freeMemory()));
        DEBUG_SERIAL_PORT.print("B: ");
        DEBUG_SERIAL_PORT.println(formatArg(input.c_str(), argp));
        DEBUG_SERIAL_PORT.flush();
    #endif
        lastLog = now;
        va_end(argp);
    }
}

#endif
