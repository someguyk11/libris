// Keep these rules aligned with backend/isbn.py; the server is authoritative.
export function normalizeIsbn(value) {
  if (typeof value !== 'string' || value.length > 64) throw new Error('Enter an ISBN-10 or ISBN-13, optionally using spaces and hyphens.')
  const compact = value.replace(/[\s-]/g, '').toUpperCase()
  let valid
  if (/^[0-9]{9}[0-9X]$/.test(compact)) valid = [...compact].reduce((sum, c, i) => sum + (10 - i) * (c === 'X' ? 10 : Number(c)), 0) % 11 === 0
  else if (/^97[89][0-9]{10}$/.test(compact)) valid = [...compact].reduce((sum, c, i) => sum + Number(c) * (i % 2 ? 3 : 1), 0) % 10 === 0
  else throw new Error('Use 10 ISBN characters or 13 digits beginning with 978 or 979.')
  if (!valid) throw new Error('The ISBN checksum is incorrect. Check the number and try again.')
  return compact
}
export function canonicalIsbn(value) {
  const compact = normalizeIsbn(value)
  if (compact.length === 13) return compact
  const body = '978' + compact.slice(0, 9)
  return body + (10 - [...body].reduce((sum, c, i) => sum + Number(c) * (i % 2 ? 3 : 1), 0) % 10) % 10
}
