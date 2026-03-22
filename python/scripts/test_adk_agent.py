import pytest
import asyncio
from unittest.mock import MagicMock, AsyncMock, patch
import sys
import io

# Import the functionality to test
from adk_agent import get_tools, run_agent

@pytest.fixture
def mock_registry():
    registry = MagicMock()
    registry.list_nodes.return_value = ["node1", "node2"]
    registry.get_signal_data.return_value = {"V(node1)": [0, 1, 2]}
    registry.compute_average_power.return_value = 0.001
    registry.plot_signal.return_value = "plot.png"
    registry.execute_commands.return_value = "success"
    return registry

def test_get_tools_structure(mock_registry):
    tools = get_tools(mock_registry)
    assert len(tools) == 5
    tool_names = [tool.func.__name__ for tool in tools ]
    assert "list_nodes" in tool_names
    assert "get_signal_data" in tool_names
    assert "compute_average_power" in tool_names
    assert "plot_signal" in tool_names
    assert "execute_commands" in tool_names

@pytest.mark.asyncio
async def test_tool_execution(mock_registry):
    tools = get_tools(mock_registry)
    # Test list_nodes wrapper
    list_nodes_tool = next(t for t in tools if t.func.__name__ == "list_nodes")
    result = await list_nodes_tool.func()
    assert result == ["node1", "node2"]
    mock_registry.list_nodes.assert_called_once()

@pytest.mark.asyncio
async def test_run_agent_initialization():
    # Mocking ADK components to verify runner setup without actual API calls
    with patch("adk_agent.ToolRegistry") as MockRegistry, \
         patch("adk_agent.Agent") as MockAgent, \
         patch("adk_agent.Runner") as MockRunner, \
         patch("adk_agent.InMemorySessionService") as MockSessionService:
        
        # Setup mocks
        mock_session_service = MockSessionService.return_value
        mock_session_service.create_session = AsyncMock(return_value=MagicMock(id="test-session"))
        
        mock_runner = MockRunner.return_value
        # mock_runner.run_async is an async generator
        async def mock_run_async(*args, **kwargs):
            yield MagicMock(thought="Thinking...", get_function_calls=lambda: [], content=None)
            yield MagicMock(thought=None, get_function_calls=lambda: [], content=MagicMock(parts=[MagicMock(text="Hello response")]))
        
        mock_runner.run_async.side_effect = mock_run_async
        
        output = io.StringIO()
        await run_agent(
            prompt_text="Hello",
            project_path="dummy.flxsch",
            output_stream=output
        )
        
        # Verify result contains the streamed output
        result = output.getvalue()
        assert "<THOUGHT>Thinking...</THOUGHT>" in result
        assert "Hello response" in result
        
        # Verify interactions
        MockRegistry.assert_called_with("dummy.flxsch")
        MockAgent.assert_called()
        MockRunner.assert_called()

if __name__ == "__main__":
    pytest.main([__file__])
