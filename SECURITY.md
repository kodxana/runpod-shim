# Security Policy

Runpod Shim is a compatibility layer, not a security boundary. It does not
grant access to devices, memory, cgroups, or files that the container cannot
already access. The shim narrows or normalizes what applications see so that
their view better matches the resources already assigned to the pod.

## Reporting A Vulnerability

Please avoid public issue details for suspected vulnerabilities until the
maintainers have had time to review them. Use GitHub private vulnerability
reporting if it is enabled for the repository, or contact the maintainers
privately with:

- A short impact summary.
- A minimal reproduction.
- Affected Runpod image/runtime details.
- Whether global preload mode was enabled.

## Supported Versions

Only the latest published release is expected to receive security fixes until
the project has a longer release history.

## Scope

Security-relevant issues include crashes or unsafe behavior caused by malformed
host/container procfs, cgroup, or NVIDIA driver state. Reports about unsupported
GPU applications or images are welcome as normal bugs unless they create a
privilege, data exposure, or denial-of-service risk.
