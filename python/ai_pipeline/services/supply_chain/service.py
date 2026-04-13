"""Supply-chain lookup for component pricing and availability.

Currently returns simulated data.  To enable real Octopart API calls,
set the ``OCTOPART_API_KEY`` environment variable and uncomment the
``requests`` block in :py:meth:`search_component`.
"""

from typing import Any, Dict, List, Optional


class SupplyChainService:
    """Look up component data from Octopart or fall back to simulated values."""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key

    def search_component(self, query: str) -> List[Dict[str, Any]]:
        """Search for a component and return basic info, price, and stock."""
        if not self.api_key:
            return self._get_simulated_data(query)

        # Real Octopart API call (requires octopart Python SDK or REST setup):
        # import requests
        # headers = {"Authorization": f"Token {self.api_key}"}
        # response = requests.post(
        #     "https://api.octopart.com/v4/endpoint",
        #     headers=headers,
        #     json={"queries": [{"mpn_or_sku": query, "reference": "q1"}]},
        # )
        # response.raise_for_status()
        # return self._parse_octopart_response(response.json())

        return self._get_simulated_data(query)

    def _get_simulated_data(self, query: str) -> List[Dict[str, Any]]:
        """Simulated data for development when no API key is present."""
        q = query.lower()
        if "tl072" in q:
            return [{
                "mpn": "TL072IP",
                "manufacturer": "Texas Instruments",
                "description": "Low-Noise JFET-Input Operational Amplifier",
                "price": 0.45,
                "currency": "USD",
                "stock": 14500,
                "distributor": "DigiKey",
                "datasheet_url": "https://www.ti.com/lit/ds/symlink/tl072.pdf"
            }]
        elif "2n3904" in q:
            return [{
                "mpn": "2N3904BU",
                "manufacturer": "onsemi",
                "description": "NPN General Purpose Transistor",
                "price": 0.08,
                "currency": "USD",
                "stock": 82000,
                "distributor": "Mouser",
                "datasheet_url": "https://www.onsemi.com/pdf/datasheet/2n3903-d.pdf"
            }]

        return [{
            "mpn": f"{query.upper()}-GENERIC",
            "manufacturer": "Generic Vendor",
            "description": f"Standard {query} component",
            "price": 1.0,
            "currency": "USD",
            "stock": 100,
            "distributor": "GenericDist",
            "datasheet_url": ""
        }]
