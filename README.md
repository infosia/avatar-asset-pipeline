# :diamond_shape_with_a_dot_inside: avatar-asset-pipeline

Avatar asset pipeline is a tool to create continuous integration build pipelines for avatar development using set of common transformation logic as a components, such as "A-pose to T-pose". The build pipelines are defined in a declarative way using JSON file.

**Work In Progress**

## Background

![The Tower of Babel](docs/The_Tower_of_Babel.jpg)
<sub>[The Tower of Babel (Vienna) By Pieter Brueghel the Elder - Google Cultural Institute](https://commons.wikimedia.org/w/index.php?curid=22178101)</sub>

> According to the story, a united human race in the generations following the Great Flood, speaking a single language and migrating eastward, comes to the land of Shinar. There they agree to build a city and a tower tall enough to reach heaven. God, observing their city and tower, confounds their speech so that they can no longer understand each other, and scatters them around the world. 

## Common transformations in practice


```json
{
  "name":"readyplayerme_Tpose",
  "description":"T-pose ReadyPlayerMe avatar",
  "pipelines":[
    {
      "name":"readyplayerme_pipeline",
      "components":[
        "apose2tpose",
        "glb_transforms_apply"
      ]
    }
  ]
}
```


![figure002](docs/figure002.png)

## Common transformations for VRM

```json
{
  "name":"glb2vrm0",
  "description":"Convert glTF binary (.glb) to VRM spec 0.0",
  "pipelines":[
    {
      "name":"gltf_pipeline",
      "components":[
        "glb_transforms_apply",
        "glb_z_reverse",
        "vrm0_fix_joint_buffer",
        "vrm0_set_materials",
        "vrm0_set_humanoid",
        "vrm0_set_default_properties"
      ]
    }
  ]
}
```

![figure001](docs/figure001.png)

## Combining multiple pipelines

```json
{
  "name":"fbx2vrm0",
  "description":"Convert FBX to VRM",
  "pipelines":[
    {
      "name":"fbx2glb",
      ...
    },
    {
      "name":"apose2tpose",
      ...
    },
    {
      "name":"glb2vrm0",
      ...
    }
  ]
}
```

![figure003](docs/figure003.png)

## Usage

Check out `pipelines` directory for working pipeline examples in practice.

```
> avatar-build -i models/input.glb -o models/output.glb --config pipelines/glb2vrm0.json --verbose 
```

## License

* Available to anybody free of charge, under the terms of MIT License (see LICENSE).

## Building

You need [Cmake](https://cmake.org/download/) and Visual Studio with C++ environment installed. There is a CMakeLists.txt file which has been tested with [Cmake](https://cmake.org/download/) on Windows. For instance in order to generate a Visual Studio 10 project, run cmake like this:


```
> mkdir build; cd build
> cmake -G "Visual Studio 10" ..
```
