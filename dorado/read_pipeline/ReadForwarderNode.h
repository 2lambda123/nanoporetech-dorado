#pragma once

#include "read_pipeline/MessageSink.h"

#include <functional>
#include <string>

namespace dorado {

// Sends on messages that are reads to the supplied callback.
class ReadForwarderNode : public MessageSink {
public:
    ReadForwarderNode(size_t max_reads, std::function<void(Message &&)> message_callback);
    ~ReadForwarderNode() { stop_input_processing(); }
    std::string get_name() const override { return "ReadForwarderNode"; }
    void terminate(const FlushOptions &) override { stop_input_processing(); }
    void restart() override { start_input_processing(&ReadForwarderNode::input_thread_fn, this); }

private:
    void input_thread_fn();

    std::function<void(Message &&)> m_message_callback;
};

}  // namespace dorado
