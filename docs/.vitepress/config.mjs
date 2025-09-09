import { globSync } from "node:fs";
import path from "node:path";
import { defineConfig } from "vitepress";

const BASE_URL = "/redis-roaring/";

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "Redis Roaring",
  lang: "en-US",
  description: "Roaring Bitmaps for Redis",
  base: BASE_URL,
  head: [
    [
      "link",
      { rel: "icon", type: "image/svg+xml", href: `${BASE_URL}logo-mini.svg` },
    ],
    [
      "link",
      { rel: "icon", type: "image/png", href: `${BASE_URL}logo-mini.png` },
    ],
  ],
  themeConfig: {
    search: {
      provider: "local",
    },

    logo: { src: "/logo-mini.svg", width: 24, height: 24 },

    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Guide", link: "/guide/what-is-roaring-bitmap" },
      { text: "Commands", link: "/commands" },
    ],

    sidebar: [
      {
        text: "Guide",
        items: [
          {
            text: "What Is Roaring Bitmap",
            link: "/guide/what-is-roaring-bitmap",
          },
          {
            text: "Getting Started",
            link: "/guide/getting-started",
          },
          {
            text: "Performance",
            link: "/guide/performance",
          },
        ],
      },
      {
        text: "Commands",
        collapsed: true,
        items: [
          { text: "Introduction", link: "/commands/" },
          ...computeSidebarCommands(),
        ],
      },
    ],

    socialLinks: [
      { icon: "github", link: "https://github.com/aviggiano/redis-roaring" },
    ],
  },
});

function computeSidebarCommands() {
  const files = globSync("./commands/*.md")
    .sort((a, b) => a.localeCompare(b))
    .filter((v) => !v.endsWith("index.md"));

  return files.map((filepath) => ({
    text: path.basename(filepath).slice(0, -3).toUpperCase(),
    link: "/" + filepath.slice(0, -3),
  }));
}
