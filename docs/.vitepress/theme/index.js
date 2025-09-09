import DefaultTheme from "vitepress/theme";
import TheContributors from "./components/TheContributors.vue";
import "./styles.css";

/** @type {import('vitepress').Theme} */
export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component("TheContributors", TheContributors);
  },
};
