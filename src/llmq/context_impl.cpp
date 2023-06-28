#include <llmq/context_impl.h>

#include <llmq/debug.h>

LLMQContextImpl::LLMQContextImpl()
: dkg_debugman_impl{std::make_unique<llmq::CDKGDebugManager>()}
{
}

LLMQContextImpl::~LLMQContextImpl() = default;

llmq::CDKGDebugManager& LLMQContextImpl::dkg_debugman()
{
    return *dkg_debugman_impl;
}
