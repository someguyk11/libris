export const memberCategories = ['Student', 'Teacher', 'Staff', 'External', 'Unassigned']
const emailPattern = /^[^\s@]+@[^\s@]+\.[^\s@]+$/

export function defaultLibraryDetails() {
  return { name: 'Libris Library', location: '', email: '', phone: '' }
}

export function validateLibraryDetails(input) {
  const details = Object.fromEntries(['name', 'location', 'email', 'phone'].map(key => [key, String(input[key] || '').trim()]))
  if (!details.name || details.name.length > 100) throw new Error('Enter a library name of up to 100 characters.')
  if (details.location.length > 250) throw new Error('Keep the location under 250 characters.')
  if (details.email && (!emailPattern.test(details.email) || details.email.length > 254)) throw new Error('Enter a valid contact email address.')
  if (details.phone && (!/^[+\d\s().-]{6,30}$/.test(details.phone) || details.phone.replace(/\D/g, '').length < 6)) throw new Error('Enter a contact phone number with at least six digits.')
  return details
}

export function validateMember(input, members, editingId = null) {
  const member = Object.fromEntries(['name', 'email', 'memberNumber', 'category'].map(key => [key, String(input[key] || '').trim()]))
  member.memberNumber = member.memberNumber.toUpperCase()
  member.category ||= 'Unassigned'
  if (!member.name || member.name.length > 100) throw new Error('Enter a member name of up to 100 characters.')
  if (!emailPattern.test(member.email) || member.email.length > 254) throw new Error('Enter a valid member email address.')
  if (!memberCategories.includes(member.category)) throw new Error('Choose a valid member category.')
  if (member.memberNumber && !/^[A-Z0-9-]{2,30}$/.test(member.memberNumber)) throw new Error('Use 2–30 letters, numbers, or hyphens for the member ID.')
  const others = members.filter(item => item.id !== editingId)
  if (others.some(item => item.email.trim().toLowerCase() === member.email.toLowerCase())) throw new Error('That member email is already registered.')
  if (member.memberNumber && others.some(item => item.memberNumber?.toUpperCase() === member.memberNumber)) throw new Error('That member ID is already registered.')
  return member
}
