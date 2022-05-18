#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

class PipelineObject:
    def __init__(self, **kwargs):
        self.data = {}
        for k,v in kwargs.items():
            self.set(k, v)

    def set(self, tag, data):
        self.data[tag] = data

    def get(self, tag):
        return self.data[tag]

    def has(self, tag):
        return tag in self.data.keys()

    def consume(self, tag):
        return self.data.pop(tag, None)

    def dump(self):
        print(self.data)

class PipelineStage:
    # The following three class variables defines the inputs and outputs of the stage. Each of them can be either a set
    # or a string (which is interpreted as a unit set)

    consumes = set()        # Data consumed by this stage. Consumed data will be unavailable to later stages.
    uses = set()            # Data used but not consumed by this stage.
    provides = set()        # Data provided by this stage.

    def run(self, obj):
        raise NotImplementedError

class PipelineEngine:
    def __init__(self, initial_data = []):
        self.stages = []
        self.initial_data = set(initial_data)
        self.available_data = set(initial_data)

    def add_stage(self, stage):
        consumes = stage.consumes if isinstance(stage.consumes, set) else {stage.consumes}
        uses = stage.uses if isinstance(stage.uses, set) else {stage.uses}
        provides = stage.provides if isinstance(stage.provides, set) else {stage.provides}

        all_uses = consumes.union(uses)
        if not all_uses.issubset(self.available_data):
            raise Exception(f"Data {all_uses - self.available_data} need by stage {stage.__class__.__name__} but not provided by the pipeline")

        self.stages.append(stage)
        self.available_data = self.available_data.difference(consumes).union(provides)

    def add_stages(self, stages):
        for stage in stages:
            self.add_stage(stage)

    def run(self, obj):
        for tag in self.initial_data:
            if not obj.has(tag):
                raise AttributeError(f"Data {tag} is needed by the pipeline but not provided by the object")

        for stage in self.stages:
            stage.run(obj)

            consumes = stage.consumes if isinstance(stage.consumes, set) else {stage.consumes}
            for tag in consumes:
                obj.consume(tag)
