from prompt import UserMessage, UserPrompt

#
# generate_dataset
#
class GenerateDataset(UserPrompt):

    def __init__(self):
        super().__init__(
            name="generate_dataset",
            description="Generate a SlideRule dataset from a scientific question",
            arguments=[
                self.QUESTION,
                {
                    "name": "mission",
                    "description": "Preferred mission, such as icesat2 or gedi",
                    "required": False
                }
            ]
        )

    def messages(self):
        return [
            UserMessage("system", "text", "You are an expert in SlideRule and Earth science remote sensing products.\n\nYour job is to select the appropriate SlideRule tool and construct the request parameters needed to generate a dataset."),
            UserMessage("system", "resource", "sliderule://mcp/datasets"),
            UserMessage("system", "resource", "sliderule://docs/python/user_guide/icesat2"),
            UserMessage("system", "resource", "sliderule://docs/python/user_guide/gedi"),
            UserMessage("system", "text", "Tool selection rules:\n\n- Raw photons -> icesat2/atl03/subset\n- Surface fitting -> icesat2/atl03/surface_fit\n- Density metrics -> icesat2/atl03/density_metrics\n- Ice sheet elevations -> icesat2/atl06/subset\n- Vegetation metrics -> icesat2/atl08/subset\n- Inland water -> icesat2/atl13/subset\n- Near shore bathymetry -> icesat2/atl24/subset"),
            UserMessage("system", "text", "When responding, return:\n\n1. The recommended tool.\n2. The reasoning.\n3. The arguments for the tool call.\n4. Any assumptions made.")
        ]
