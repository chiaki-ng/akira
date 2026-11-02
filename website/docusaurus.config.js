// @ts-check
import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'Akira',
  tagline: 'PlayStation Remote Play on the Nintendo Switch',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  url: 'https://chiaki-ng.github.io',
  baseUrl: '/akira/',

  organizationName: 'chiaki-ng',
  projectName: 'akira',

  onBrokenLinks: 'throw',

  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'throw',
    },
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          routeBasePath: '/',
          sidebarPath: './sidebars.js',
          editUrl: 'https://github.com/chiaki-ng/akira/tree/next/website/',
          lastVersion: '0.6.0',
          versions: {
            current: {
              label: '0.7.0',
              path: '0.7.0',
              banner: 'unreleased',
            },
          },
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      colorMode: {
        respectPrefersColorScheme: true,
      },
      navbar: {
        title: 'Akira',
        logo: {
          alt: 'Akira',
          src: 'img/logo.svg',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'docs',
            position: 'left',
            label: 'Docs',
          },
          {
            type: 'docsVersionDropdown',
            position: 'right',
          },
          {
            href: 'https://github.com/chiaki-ng/akira',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              {label: 'Setup', to: '/setup/registering'},
              {label: 'Features', to: '/features'},
              {label: 'Developing', to: '/developing'},
            ],
          },
          {
            title: 'Related',
            items: [
              {label: 'chiaki-ng', href: 'https://github.com/streetpea/chiaki-ng'},
              {label: 'borealis', href: 'https://github.com/xfangfang/borealis'},
            ],
          },
          {
            title: 'More',
            items: [
              {label: 'GitHub', href: 'https://github.com/chiaki-ng/akira'},
            ],
          },
        ],
        copyright: `Akira. Built with Docusaurus.`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
      },
    }),
};

export default config;
