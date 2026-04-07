# 2026-04-03: Web Client release v4.3.1

:::{note}
SlideRule Web Client v4.3.1 adds map sampling controls, OAuth 2.1 support, and wider elevation plots.
:::

## Summary

**SlideRule Web Client v4.3.1 Release Notes**

Changes since v4.0.3

**Infrastructure**
- OAuth 2.1 - Updated authentication to match new server OAuth 2.1 requirements, including support for standard and custom Dynamic Client Registration
- TLS Certificates - Upgraded cert version to current recommended

**New Features**
- Map Sampling Controls - A new "Show All" button appears in the map header when a dataset is being sampled, opening a slider to choose how many points to render. After rendering, you're prompted to save the value as your new default threshold. A "Sample" button lets you dial it back down. The setting persists across page reloads and carries over when switching between records.
- Region Selection Component - Added a component for selecting regions, designed for use by AI agent workflows
- Wider Elevation Plots - Elevation plots can now be widened horizontally for better data exploration

**Improvements**
- News Snippets - Added article preview snippets to the News feature
- iPad Styling - Cleaned up layout and styling for iPad
- Point Count Accuracy - Fixed discrepancy between point counts shown on the Analysis page vs. the Records page (#1011)
- Raster Parameters - Fixed raster parameters not always showing up (#1029)
- SQL Sanitization - Refactored SQL queries to improve input sanitization (#1023)
- Redirect Fix - Fixed redirect to request page for client subdomain (#1042)
