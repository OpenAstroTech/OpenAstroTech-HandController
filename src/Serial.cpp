#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"

// Handle the commands coming from a client app like OATControl or ASCOM
MessageJob* processSerialFromClient()
{
    static String inCmd = "";

    while (Serial.available() > 0)
    {
        char ch = Serial.read();

        if (ch == 0x06)
        {
            Serial.print('P');
        }
        else if (ch == '#')
        {
            // LOG(DEBUG_SERIAL, "[SERIAL]: ReceivedCommand(%d chars): [%s]", inCmd.length(), inCmd.c_str());
            inCmd.trim();
            auto job = new MessageJob(inCmd + "#", JobSource::FromClient);
            inCmd = "";
            return job;
        }
        else
        {
            inCmd += ch;
        }

    }
    return nullptr;
}

// Handle the command replies coming from the mount
String processSerialFromMount(MessageJob* activeJob, SoftwareSerial* serialPort)
{
    static String replyAccumulator = "";
    static bool firstHash = true;
    String reply = "";
    while (serialPort->available() > 0)
    {
        char ch = serialPort->read();
        switch (activeJob->getCommandType())
        {
            case CommandType::NoReply:
                LOG(DEBUG_MOUNT, "Active Job [%s] received a reply [%s], but none was expected.", activeJob->getCommand().c_str(), reply.c_str());
                break;

            case CommandType::NumberReply:
                reply = String(ch);
                break;

            case CommandType::StringReply:
                if (ch == '#')
                {
                    reply = replyAccumulator + "#";
                    replyAccumulator = "";
                }
                else
                {
                    replyAccumulator += ch;
                }
                break;

            case CommandType::DoubleStringReply:
                if (ch == '#')
                {
                    if (firstHash)
                    {
                        firstHash = false;
                        replyAccumulator += ch;
                    }
                    else
                    {
                        reply = replyAccumulator + "ch";
                        replyAccumulator = "";
                        firstHash = true;
                    }
                }
                else
                {
                    replyAccumulator += ch;
                }
                break;
        }
        if (!reply.isEmpty())
        {
            // If we have a reply, return even if there are more serial characters waiting.... they are not from this job.
            break;
        }
    }
    reply.trim();
    return reply;
}
