// Unlighthouse v1 supports --device mobile,desktop in one run;
// 0.17.4 (Debian 13 pin) needs two passes (Makefile: --desktop / --mobile).
export default {
	scanner: {
		throttle: true,
		samples: 5,
	},
	puppeteerClusterOptions: {
		maxConcurrency: 1,
	},
	puppeteerOptions: {
		args: [
			'--no-sandbox',
			'--disable-dev-shm-usage',
			'--disable-gpu',
			'--disable-background-timer-throttling',
			'--disable-backgrounding-occluded-windows',
			'--disable-renderer-backgrounding',
		],
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
