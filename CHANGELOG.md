# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Loading a schematic while joined to a server failed with "World registry is unavailable" (#11). Palette resolution, content reload, session restore and synchronous selection export now use the client Level instead of the in-process server Level.
