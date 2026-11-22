#include <Arduino.h>
#include <WString.h>

#include "Serial.hpp"
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

// Reply bytes of the job in flight that have not formed a complete reply yet.
static String replyAccumulator = "";
static bool firstHash = true;  // DoubleStringReply: waiting for the first of two '#'

// Forgets a reply that was only partly received, so that its bytes cannot end
// up in front of the reply to the next command. Called when a job is
// abandoned while its reply is incomplete.
void discardPartialReply()
{
    replyAccumulator = "";
    firstHash = true;
}

// Handle the command replies coming from the mount. anyByte, when given, is
// set as soon as the mount has sent anything for this job.
String processSerialFromMount(MessageJob* activeJob, ISerial* serialPort, bool* anyByte)
{
    String reply = "";
    while (serialPort->connected() && serialPort->available() > 0)
    {
        char ch;
        serialPort->read(&ch,1);
        if (anyByte != nullptr)
        {
            *anyByte = true;
        }
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
                        reply = replyAccumulator + "#";
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
        if (!reply.length() == 0)
        {
            // If we have a reply, return even if there are more serial characters waiting.... they are not from this job.
            break;
        }
    }
    reply.trim();
    return reply;
}
