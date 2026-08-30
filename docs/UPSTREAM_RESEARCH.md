# Upstream research notes

## Quilibrium protocol source

The source architecture is explicitly multi-process and sharded: core 0 runs global consensus; worker processes run app consensus and Token/Hypergraph/Compute execution. Hypergraph state is maintained per shard and uses CRDT semantics. The source tree includes BLS48-581, Bulletproofs, VDF, verifiable encryption, MPC/channel components and FFI crates.

The protocol protobuf tree currently contains `application.proto`, `channel.proto`, `compute.proto`, `global.proto`, `hypergraph.proto`, `keys.proto`, `node.proto`, `proxy.proto`, `ferret_proxy.proto` and `token.proto` plus generated Go/gRPC gateway outputs.

## Hosted APIs

The official API documentation currently names QStorage and QKMS as available hosted services. QStorage is S3-compatible. QKMS is KMS-compatible and its separate official SDK adds embedded-wallet/threshold-MPC workflows backed by QNZM authentication/IAM.

## HyperSnap

HyperSnap is a Farcaster-focused API. Its docs separate read API, signed EIP-712 management operations, webhooks, and mini-app notification flows. The docs explicitly provide LLM-oriented full-spec artifacts and recommend pinning source/docs revisions in production when API drift matters.

## Licensing boundary

The Quilibrium monorepo carries GNU AGPLv3. The current QKMS SDK README states `Copyright Quilibrium, Inc. All rights reserved.` This SDK therefore models externally observable contracts and documented behavior instead of copying upstream implementation code. Redistribution of generated code derived from upstream schemas should receive a dedicated license review before a public release.
