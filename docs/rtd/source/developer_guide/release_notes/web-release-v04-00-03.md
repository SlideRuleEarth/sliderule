# 2026-02-25: Web Client release v4.0.3

:::{note}
SlideRule Web Client v4.0.3 has new Landing page
:::

## Summary

**🚀 SlideRule Web Client v4.0.3 Release Notes**

Changes since v3.8.0

**Infrastructure**
- CloudFront + Route 53 terraform modules added to support hosting the landing page at the root domain

**New Features**
- Landing Page - The web client now serves as the SlideRule Earth landing page at slideruleearth.io, featuring a hero section with wallpaper image, About/Contact info panels, and a News tab that pulls articles directly from the SlideRule documentation site
- Home Button - A new Home button in the app bar navigates back to the landing page from anywhere in the app
- Docs Button - The old "About" menu has been replaced with a streamlined "Docs" button that links directly to the SlideRule documentation site
- Feedback Menu - The Feedback button now offers a dropdown with options to email support or open a GitHub issue directly

**Improvements**
- Full-Width Analysis Map - The analysis map now uses the entire available width instead of a fixed 45vw, providing more room for map exploration
- Cleaner Analysis Layout - Removed unnecessary card wrapper and margins from the analysis view; the 3D panel is now hidden when not in use instead of taking up empty space
- Status Tooltips - All request statuses now show tooltip details on hover, including "pending" statuses that previously had no tooltip