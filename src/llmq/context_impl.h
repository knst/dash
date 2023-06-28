#ifndef BITCOIN_LLMQ_CONTEXT_IMPL_H
#define BITCOIN_LLMQ_CONTEXT_IMPL_H

#include <memory>

namespace llmq
{
class CDKGDebugManager;
} // namespace llmq

class LLMQContextImpl
{
    const std::unique_ptr<llmq::CDKGDebugManager> dkg_debugman_impl;
public:
    LLMQContextImpl();
    ~LLMQContextImpl();
    llmq::CDKGDebugManager& dkg_debugman();
};

#endif

