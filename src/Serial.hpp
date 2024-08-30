#pragma once
#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"

MessageJob* processSerialFromClient();

// Handle the command replies coming from the mount
String processSerialFromMount(MessageJob* activeJob, SoftwareSerial* serialPort);