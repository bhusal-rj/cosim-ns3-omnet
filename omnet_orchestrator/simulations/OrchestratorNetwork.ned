package simulations;

network OrchestratorNetwork
{
    submodules:
        orchestrator: Orchestrator {
            parameters:
                port = default(9999);
        }
}