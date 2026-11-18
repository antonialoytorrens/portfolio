/** Docker/CI Chromium needs these flags (no user namespaces / sandbox). */
export default {
	puppeteerOptions: {
		args: ['--no-sandbox', '--disable-dev-shm-usage'],
	},
	lighthouseOptions: {
		onlyCategories: [
			'performance',
			'accessibility',
			'best-practices',
			'seo',
		],
	},
}
