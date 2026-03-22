pub struct SystemStatsProvider;

// The actual polling logic is directly in DataProviderEngine for simplicity,
// since sysinfo::System needs to be owned by the engine for refresh cycles.
// This module exists as a namespace placeholder for future provider extraction.
