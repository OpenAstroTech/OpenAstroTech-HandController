#pragma once
#include <Arduino.h>
#include <WString.h>
#include <SoftwareSerial.h>

#include "Utility.hpp"

enum JobSource
{
    None,
    FromClient,
    FromController
};

enum CommandType
{
    NoReply,
    NumberReply,
    StringReply,
    DoubleStringReply
};

class MessageJob
{
    friend class JobQueue;

private:
    static long _currentId;
    long _id;
    CommandType _type;
    JobSource _source;
    String _command;
    MessageJob* _next;
    MessageJob* _prev;

public:
    MessageJob(String command, JobSource source)
    {
        _id = _currentId++;
        _command = command;
        _type = parseCommandType(command);
        _source = source;
        _next = nullptr;
        _prev = nullptr;
    }

    CommandType getCommandType() const
    {
        return _type;
    }

    String getCommand() const
    {
        return _command;
    }

    JobSource getSource() const
    {
        return _source;
    }

private:
    CommandType parseCommandType(String inCmd) const;
};

class JobQueue
{
    MessageJob* _activeJob;
    MessageJob* _rootJob;
    MessageJob* _lastJob;

public:
    JobQueue()
    {
        _rootJob = new MessageJob("root", JobSource::None);
        _lastJob = _rootJob;
        _activeJob = nullptr;
    }

    ~JobQueue()
    {
        MessageJob* job;

        // Dequeue and delete all jobs to free memory
        while ((job = dequeue()) != nullptr)
        {
            delete job; 
        }
        delete _rootJob;
    }

    bool hasJob()
    {
        return _lastJob != _rootJob;
    }

    // Adds job at the end of the queue
    void enqueue(MessageJob* job)
    {
        _lastJob->_next = job;
        job->_next = nullptr;
        job->_prev = _lastJob;
        _lastJob = job;
    }

    // Removes head of the queue
    MessageJob* dequeue()
    {
        if (_lastJob == _rootJob)
        {
            LOG(DEBUG_JOBS, "Dequeue called, but queue is empty");
            return nullptr;
        }
        MessageJob* returnJob = _rootJob->_next;
        LOG(DEBUG_JOBS, "Dequeue called, returning %s.", returnJob->getCommand().c_str());
        _rootJob->_next = returnJob->_next;
        if (_rootJob->_next != nullptr)
        {
            _rootJob->_next->_prev = _rootJob;
        }
        else
        {
            _lastJob = _rootJob;  // Reset _lastJob when queue is empty
        }
        LOG(DEBUG_JOBS, "Dequeue called, rootJob is now %s.", _rootJob->getCommand().c_str());

        return returnJob;
    }

    MessageJob* peek()
    {
        if (_lastJob == _rootJob)
        {
            return nullptr;
        }
        return _rootJob->_next;
    }

    // void setActiveJob(MessageJob *job) { _activeJob = job; }
    // MessageJob *getActiveJob() { return _activeJob; }
};
