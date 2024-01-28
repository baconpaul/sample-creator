/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#include "SampleCreator.hpp"
#include "SampleCreatorSkin.hpp"

rack::Plugin *pluginInstance;
namespace baconpaul::samplecreator
{
SampleCreatorSkin sampleCreatorSkin;
}

__attribute__((__visibility__("default"))) void init(rack::Plugin *p)
{
    pluginInstance = p;

    p->addModel(sampleCreatorModel);
}
